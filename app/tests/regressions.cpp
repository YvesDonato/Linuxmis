#include <QObject>
#ifndef Q_MOC_RUN
#include <QtTest>
#include <QGuiApplication>
#include <QTemporaryDir>
#include "backend/systemproperties.h"
#include "backend/computerseeker.h"
#include "backend/computermanager.h"
#include "cli/startstream.h"
#include "streaming/video/ffmpeg-renderers/pacer/pacer.h"
#include "streaming/video/ffmpeg-renderers/pacer/waylandvsyncsource.h"
#include "streaming/video/ffmpeg-renderers/eglvid.h"
#include "streaming/mangonativesplit.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QStandardPaths>
#include <SDL_syswm.h>
#endif

static QVector<NvComputer*> hosts;
static QVector<NvApp> apps;
static int pollingRefs, appRequests, quitRequests, manualRequests;
static bool failAppList, failContext;
static const char* failedThread = "";
static int displays = 2;

static QJsonObject nativeClient(int width = 1920, int height = 1080)
{
    return {{"pid", QCoreApplication::applicationPid()}, {"appid", "com.linuxmis.linuxmis"},
            {"title", MangoNativeSplit::WindowTitle}, {"monitor", "DP-2"},
            {"is_floating", false}, {"is_fullscreen", false},
            {"width", width}, {"height", height}, {"x", 3398 - width}, {"y", (1440 - height) / 2},
            {"reservation", QJsonObject{{"status", "accepted"}, {"requested_width", width},
             {"requested_height", height}, {"scale", 1}, {"reason", QJsonValue()},
             {"usable_bounds", QJsonObject{{"x", 0}, {"y", 0}, {"width", 3398}, {"height", 1440}}}}}};
}

static QByteArray nativeReply(QJsonArray clients)
{
    return QJsonDocument(QJsonObject{{"clients", clients}}).toJson(QJsonDocument::Compact);
}

// Linker wrappers isolate networking and inject platform failures. All state machines
// and queue/display management below are the actual application implementations.
extern "C" {
QVector<NvComputer*> __wrap__ZN15ComputerManager12getComputersEv(ComputerManager*) { return hosts; }
void __wrap__ZN15ComputerManager12startPollingEv(ComputerManager*) { ++pollingRefs; }
void __wrap__ZN15ComputerManager16stopPollingAsyncEv(ComputerManager*) { --pollingRefs; }
void __wrap__ZN15ComputerManager18addNewHostManuallyE7QString(ComputerManager*, QString) { ++manualRequests; }
void __wrap__ZN15ComputerManager14quitRunningAppEP10NvComputer(ComputerManager*, NvComputer*) { ++quitRequests; }
void __wrap__ZN13CompatFetcher5startEv(CompatFetcher*) {}
QVector<NvApp> __wrap__ZN6NvHTTP10getAppListEv(NvHTTP*) {
    ++appRequests;
    if (failAppList) throw std::runtime_error("test connection failure");
    return apps;
}
int __wrap_SDL_GetNumVideoDisplays() { return displays; }
int __wrap_SDL_GetNumDisplayModes(int) { return 0; }
bool __wrap__ZN11StreamUtils20getNativeDesktopModeEiP15SDL_DisplayModeP8SDL_Rect(int index, SDL_DisplayMode* mode, SDL_Rect* area) {
    if (index == 0 && displays == 2) return false;
    *mode = {};
    mode->w = 1920;
    mode->h = 1080;
    mode->refresh_rate = 144;
    *area = {0, 0, 1920, 1080};
    return true;
}
int __wrap__ZN11StreamUtils21getDisplayRefreshRateEP10SDL_Window(SDL_Window*) { return 144; }
SDL_bool __wrap_SDL_GetWindowWMInfo(SDL_Window*, SDL_SysWMinfo* info) {
    info->subsystem = SDL_SYSWM_WAYLAND;
    return SDL_TRUE;
}
SDL_Thread* __real_SDL_CreateThread(SDL_ThreadFunction, const char*, void*);
SDL_Thread* __wrap_SDL_CreateThread(SDL_ThreadFunction fn, const char* name, void* data) {
    if (strcmp(name, failedThread) == 0) {
        SDL_SetError("injected thread failure");
        return nullptr;
    }
    return __real_SDL_CreateThread(fn, name, data);
}
int __real_SDL_GL_MakeCurrent(SDL_Window*, SDL_GLContext);
int __wrap_SDL_GL_MakeCurrent(SDL_Window* window, SDL_GLContext context) {
    return failContext ? SDL_SetError("injected context failure") : __real_SDL_GL_MakeCurrent(window, context);
}
}

// No compositor connection is needed to test Pacer's thread/queue lifecycle.
WaylandVsyncSource::WaylandVsyncSource(Pacer* pacer)
    : m_Pacer(pacer), m_Display(nullptr), m_Surface(nullptr), m_Callback(nullptr) {}
WaylandVsyncSource::~WaylandVsyncSource() = default;
bool WaylandVsyncSource::initialize(SDL_Window*, int) { return true; }
bool WaylandVsyncSource::isAsync() { return true; }

class TestRenderer : public IFFmpegRenderer {
public:
    TestRenderer() : IFFmpegRenderer(RendererType::Unknown) {}
    bool initialize(PDECODER_PARAMETERS) override { return true; }
    bool prepareDecoderContext(AVCodecContext*, AVDictionary**) override { return true; }
    void prepareToRender() override { prepared = true; }
    void waitToRender() override { if (!prepared) ++unpreparedCalls; }
    void renderFrame(AVFrame*) override { ++frames; }
    bool canExportEGL() override { return true; }
    std::atomic<int> frames{0};
    std::atomic<bool> prepared{false};
    std::atomic<int> unpreparedCalls{0};
};

class Regressions : public QObject {
    Q_OBJECT
private slots:
    void nativeSplitValidatesCompositorGeometry() {
        using ReservationState = MangoNativeSplit::ReservationState;
        auto check = [](QJsonObject client, QSize size) {
            return MangoNativeSplit::parseReply(nativeReply({client}), QCoreApplication::applicationPid(), size);
        };
        for (QSize size : {QSize(1920, 1080), QSize(1920, 1440), QSize(2758, 1440)}) {
            QCOMPARE(check(nativeClient(size.width(), size.height()), size).status, ReservationState::Accepted);
        }
        for (QSize size : {QSize(2759, 1440), QSize(1920, 1441), QSize(0, 1080)}) {
            QCOMPARE(check(nativeClient(size.width(), size.height()), size).status, ReservationState::Rejected);
        }
        auto client = nativeClient();
        client["x"] = 0;
        QCOMPARE(check(client, QSize(1920, 1080)).status, ReservationState::Rejected);
        client = nativeClient();
        client["width"] = 1920.5;
        QCOMPARE(check(client, QSize(1920, 1080)).status, ReservationState::Rejected);
        client = nativeClient();
        client["monitor"] = "eDP-1";
        QCOMPARE(check(client, QSize(1920, 1080)).status, ReservationState::Rejected);
        client = nativeClient();
        client.remove("reservation");
        QCOMPARE(check(client, QSize(1920, 1080)).status, ReservationState::Rejected);
        QCOMPARE(check(nativeClient(), QSize(1280, 720)).status, ReservationState::Pending);
        QCOMPARE(MangoNativeSplit::parseReply(nativeReply({}), QCoreApplication::applicationPid(), QSize(1920, 1080)).status, ReservationState::Pending);
        QCOMPARE(MangoNativeSplit::parseReply("broken JSON", 1, QSize(1920, 1080)).status, ReservationState::Rejected);
        QCOMPARE(MangoNativeSplit::parseReply(nativeReply({nativeClient(), nativeClient()}), QCoreApplication::applicationPid(), QSize(1920, 1080)).status, ReservationState::Rejected);
        for (const char* reason : {"too_large", "output_unavailable", "unsupported_scale", "region_occupied"}) {
            client = nativeClient();
            client["reservation"] = QJsonObject{{"status", "rejected"}, {"reason", reason}};
            auto result = check(client, QSize(1920, 1080));
            QCOMPARE(result.status, ReservationState::Rejected);
            QVERIFY(!result.error.isEmpty());
        }
    }

    void nativeSplitQueryAndPresentationGate() {
        struct RestorePath {
            QByteArray path = qgetenv("PATH");
            ~RestorePath() { qputenv("PATH", path); }
        } restore;
        QTemporaryDir executables;
        QVERIFY(executables.isValid());
        const auto shell = QStandardPaths::findExecutable("sh").toUtf8();
        QVERIFY(!shell.isEmpty());
        auto script = [&](QByteArray body) {
            QFile file(executables.filePath("mmsg"));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
            file.write("#!" + shell + "\n" + body + "\n");
            return file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        };
        QVERIFY(script("printf '%s' '" + nativeReply({nativeClient()}) + "'"));
        qputenv("PATH", executables.path().toUtf8() + ":" + restore.path);
        QCOMPARE(SDL_InitSubSystem(SDL_INIT_VIDEO), 0);
        SDL_Window* window = SDL_CreateWindow("native test", 0, 0, 1920, 1080, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
        QVERIFY(window);
        {
            MangoNativeSplit split(window, QSize(1920, 1080));
            QVERIFY(!split.canPresentFrame(1920, 1080));
            split.begin();
            QElapsedTimer timer;
            timer.start();
            while (!split.accepted() && timer.elapsed() < 2500) {
                QVERIFY2(split.poll().isEmpty(), "native split query failed");
                QTest::qWait(10);
            }
            QVERIFY(split.accepted());
            QVERIFY(split.canPresentFrame(1920, 1080));
            SDL_FlushEvents(SDL_USEREVENT, SDL_USEREVENT);
            QVERIFY(!split.canPresentFrame(1280, 720));
            QVERIFY(!split.canPresentFrame(1280, 720));
            SDL_Event event;
            QCOMPARE(SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_USEREVENT, SDL_USEREVENT), 1);
            QCOMPARE(event.user.code, MangoNativeSplit::RecheckEvent);
            QCOMPARE(int(intptr_t(event.user.data1)), 1280);
            QCOMPARE(SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_USEREVENT, SDL_USEREVENT), 0);

            QVERIFY(script("printf '%s' '" + nativeReply({}) + "'"));
            split.begin();
            QString error;
            timer.restart();
            while (error.isEmpty() && timer.elapsed() < 2500) {
                error = split.poll();
                QTest::qWait(10);
            }
            QVERIFY(error.contains("Timed out"));
            QVERIFY(!split.accepted());

            QVERIFY(script("exec sleep 5"));
            split.begin();
            error.clear();
            timer.restart();
            while (error.isEmpty() && timer.elapsed() < 1000) {
                error = split.poll();
                QTest::qWait(10);
            }
            QVERIFY(error.contains("did not respond"));
        }
        QVERIFY(!MangoNativeSplit::fromWindow(window));
        SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void init() {
        hosts.clear();
        apps.clear();
        pollingRefs = appRequests = quitRequests = manualRequests = 0;
        failAppList = failContext = false;
        failedThread = "";
    }

    void displayRefreshBalancesSdlAndReplacesLists() {
        SystemProperties properties;
        QCOMPARE(SDL_WasInit(SDL_INIT_VIDEO), 0u);
        QVERIFY(!properties.property("hasHardwareAcceleration").toBool());
        QCOMPARE(properties.property("maximumResolution").toSize(), QSize(0, 0));
        displays = 2;
        properties.refreshDisplays();
        QCOMPARE(properties.getDisplayCount(), 2);
        QVERIFY(properties.getNativeResolution(0).isEmpty());
        QCOMPARE(properties.getRefreshRate(1), 144);
        QCOMPARE(SDL_WasInit(SDL_INIT_VIDEO), 0u);
        displays = 1;
        properties.refreshDisplays();
        QCOMPARE(properties.getDisplayCount(), 1);
        QCOMPARE(properties.getRefreshRate(0), 144);
        QCOMPARE(properties.getRefreshRate(1), 0);
        QVERIFY(properties.getSafeAreaResolution(1).isEmpty());
        QCOMPARE(SDL_WasInit(SDL_INIT_VIDEO), 0u);

        QCOMPARE(SDL_InitSubSystem(SDL_INIT_VIDEO), 0);
        properties.refreshDisplays();
        QVERIFY(SDL_WasInit(SDL_INIT_VIDEO));
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        QCOMPARE(SDL_WasInit(SDL_INIT_VIDEO), 0u);

        qputenv("SDL_VIDEODRIVER", "nonexistent-test-driver");
        properties.refreshDisplays();
        QCOMPARE(properties.getDisplayCount(), 0);
        QCOMPARE(properties.getRefreshRate(0), 0);
        QSignalSpy probed(&properties, &SystemProperties::decoderInfoChanged);
        properties.ensureDecoderInfo();
        properties.ensureDecoderInfo();
        QCOMPARE(probed.count(), 1);
        QCOMPARE(properties.property("maximumResolution").toSize(), QSize(0, 0));
        qputenv("SDL_VIDEODRIVER", "dummy");
    }

    void cachedHostFoundOnceAndPollingReleased() {
        auto preferences = StreamingPreferences::get();
        ComputerManager manager(preferences);
        NvComputer host{};
        host.name = "PC";
        host.state = NvComputer::CS_ONLINE;
        host.activeAddress = NvAddress("127.0.0.1", 47989);
        hosts = {&host};
        {
            ComputerSeeker seeker(&manager, "pc");
            QSignalSpy found(&seeker, &ComputerSeeker::computerFound);
            seeker.start(100);
            QTRY_COMPARE(found.count(), 1);
            emit manager.computerStateChanged(&host);
            QCOMPARE(found.count(), 1);
            QCOMPARE(pollingRefs, 0);
            QCOMPARE(manualRequests, 0);
        }
        QCOMPARE(pollingRefs, 0);
        {
            ComputerSeeker seeker(&manager, "missing");
            seeker.start(1000);
            QCOMPARE(pollingRefs, 1);
        }
        QCOMPARE(pollingRefs, 0);
        ComputerSeeker seeker(&manager, "missing");
        QSignalSpy timeout(&seeker, &ComputerSeeker::errorTimeout);
        seeker.start(1);
        QTRY_COMPARE(timeout.count(), 1);
        QCOMPARE(pollingRefs, 0);
    }

    void explicitAppFetchAndSelectedHostOnly() {
        auto preferences = StreamingPreferences::get();
        ComputerManager manager(preferences);
        NvComputer host{}, other{};
        host.name = "PC";
        host.state = other.state = NvComputer::CS_ONLINE;
        host.pairState = NvComputer::PS_PAIRED;
        host.activeAddress = NvAddress("127.0.0.1", 47989);
        host.activeHttpsPort = 47984;
        host.currentGameId = 2;
        hosts = {&host};
        NvApp desktop, running;
        desktop.id = 1;
        desktop.name = "Desktop";
        running.id = 2;
        running.name = "Running";
        apps = {desktop, running};
        host.appList = apps;
        {
            CliStartStream::Launcher launcher("pc", "desktop", preferences);
            QSignalSpy prompt(&launcher, &CliStartStream::Launcher::appQuitRequired);
            QSignalSpy session(&launcher, &CliStartStream::Launcher::sessionCreated);
            QSignalSpy failure(&launcher, &CliStartStream::Launcher::failed);
            launcher.execute(&manager);
            QTRY_COMPARE(prompt.count(), 1);
            QCOMPARE(appRequests, 1);
            QCOMPARE(pollingRefs, 1);
            emit manager.computerStateChanged(&host);
            QCOMPARE(prompt.count(), 1);
            launcher.quitRunningApp();
            launcher.quitRunningApp();
            QCOMPARE(quitRequests, 1);
            other.currentGameId = 0;
            emit manager.computerStateChanged(&other);
            QCOMPARE(session.count(), 0);
            QCOMPARE(prompt.count(), 1);
            emit manager.quitAppCompleted(QString("test quit failure"));
            QCOMPARE(failure.count(), 1);
            QCOMPARE(pollingRefs, 0);
        }
        failAppList = true;
        CliStartStream::Launcher launcher("pc", "desktop", preferences);
        QSignalSpy failure(&launcher, &CliStartStream::Launcher::failed);
        launcher.execute(&manager);
        QTRY_COMPARE(failure.count(), 1);
        QVERIFY(failure.first().first().toString().contains("test connection failure"));
        QCOMPARE(pollingRefs, 0);
    }

    void threadFailuresAndIdleTeardown() {
        TestRenderer renderer;
        VIDEO_STATS stats{};
        for (const char* name : {"PacerVsync", "PacerRender"}) {
            failedThread = name;
            Pacer pacer(&renderer, &stats);
            QVERIFY(!pacer.initialize(nullptr, 144, true));
        }
        failedThread = "";
        for (int i = 0; i < 50; ++i) {
            renderer.prepared = false;
            Pacer pacer(&renderer, &stats);
            QVERIFY(pacer.initialize(nullptr, 144, false));
            QVERIFY(renderer.prepared);
        }
        Pacer pacer(&renderer, &stats);
        QVERIFY(pacer.initialize(nullptr, 144, false));
        AVFrame* frame = av_frame_alloc();
        QVERIFY(frame);
        frame->pkt_dts = SDL_GetTicks();
        pacer.submitFrame(frame);
        QTRY_COMPARE(renderer.frames.load(), 1);
        QCOMPARE(renderer.unpreparedCalls.load(), 0);
    }

    void failedContextRequestsOneReset() {
        QCOMPARE(SDL_InitSubSystem(SDL_INIT_EVENTS), 0);
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        TestRenderer backend;
        EGLRenderer renderer(&backend);
        failContext = true;
        renderer.prepareToRender();
        renderer.waitToRender();
        renderer.renderFrame(nullptr);
        SDL_Event events[2];
        QCOMPARE(SDL_PeepEvents(events, 2, SDL_GETEVENT, SDL_RENDER_TARGETS_RESET, SDL_RENDER_TARGETS_RESET), 1);
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir state;
    if (!state.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", state.path().toUtf8());
    qputenv("XDG_CACHE_HOME", state.path().toUtf8());
    qputenv("XDG_DATA_HOME", state.path().toUtf8());
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("SDL_VIDEODRIVER", "dummy");
    qunsetenv("WAYLAND_DISPLAY");
    qunsetenv("DISPLAY");
    QGuiApplication app(argc, argv);
    app.setOrganizationName("linuxmis-tests");
    app.setApplicationName("regressions");
    Regressions tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "regressions.moc"
