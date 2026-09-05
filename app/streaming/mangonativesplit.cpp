#include "mangonativesplit.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <cmath>

namespace {
constexpr const char* WindowData = "linuxmis.mango-native-split";
constexpr int DeadlineMs = 2000;
constexpr int QueryTimeoutMs = 250;

bool integer(const QJsonValue& value, int& result)
{
    double number = value.toDouble(-1);
    if (!value.isDouble() || !std::isfinite(number) || number < -32768 ||
            number > 32767 || std::floor(number) != number) {
        return false;
    }
    result = static_cast<int>(number);
    return true;
}

QSize drawableSize(SDL_Window* window)
{
    int width = 0, height = 0;
#if SDL_VERSION_ATLEAST(2, 26, 0)
    SDL_GetWindowSizeInPixels(window, &width, &height);
#endif
    return QSize(width, height);
}
}

bool MangoNativeSplit::isEnabled()
{
#ifdef Q_OS_LINUX
    return qgetenv("LINUXMIS_MANGO_SPLIT") == "1" &&
            QString::fromLocal8Bit(qgetenv("XDG_CURRENT_DESKTOP")).split(':')
                .contains(QStringLiteral("mango"), Qt::CaseInsensitive);
#else
    return false;
#endif
}

MangoNativeSplit* MangoNativeSplit::fromWindow(SDL_Window* window)
{
    return window ? static_cast<MangoNativeSplit*>(SDL_GetWindowData(window, WindowData)) : nullptr;
}

MangoNativeSplit::Reply MangoNativeSplit::parseReply(const QByteArray& json, qint64 pid, QSize size)
{
    auto invalid = [] {
        return Reply{ReservationState::Rejected, tr("Mango did not confirm a pixel-exact split. Install the matching patched Mango package and restart the Mango session.")};
    };
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject() ||
            !document.object().value("clients").isArray()) {
        return invalid();
    }

    QJsonObject client;
    int matches = 0;
    for (const auto& value : document.object().value("clients").toArray()) {
        const auto candidate = value.toObject();
        if (candidate.value("pid").toDouble(-1) == pid &&
                candidate.value("appid") == QStringLiteral("com.linuxmis.linuxmis") &&
                candidate.value("title") == QString::fromLatin1(WindowTitle)) {
            client = candidate;
            ++matches;
        }
    }
    if (!matches) return {ReservationState::Pending, {}};
    if (matches != 1 || !client.value("reservation").isObject()) return invalid();

    const auto reservation = client.value("reservation").toObject();
    const auto status = reservation.value("status").toString();
    if (status == "pending") return {ReservationState::Pending, {}};
    if (status == "rejected") {
        const auto reason = reservation.value("reason").toString();
        if (reason == "too_large") {
            return {ReservationState::Rejected, tr("The stream is too large for a pixel-exact split on DP-2. Choose a smaller resolution that leaves at least 640 pixels for local apps.")};
        }
        if (reason == "output_unavailable" || reason == "unsupported_scale") {
            return {ReservationState::Rejected, tr("The pixel-exact split requires DP-2 to be enabled at scale 1.")};
        }
        if (reason == "region_occupied") {
            return {ReservationState::Rejected, tr("Another stream already occupies the native split. Close it before starting another stream on this workspace.")};
        }
        return invalid();
    }
    if (status != "accepted") return invalid();

    int width, height, x, y, bx, by, bw, bh, requestedWidth, requestedHeight;
    const auto bounds = reservation.value("usable_bounds").toObject();
    if (!integer(client.value("width"), width) || !integer(client.value("height"), height) ||
            !integer(client.value("x"), x) || !integer(client.value("y"), y) ||
            !integer(bounds.value("x"), bx) || !integer(bounds.value("y"), by) ||
            !integer(bounds.value("width"), bw) || !integer(bounds.value("height"), bh) ||
            !integer(reservation.value("requested_width"), requestedWidth) ||
            !integer(reservation.value("requested_height"), requestedHeight) ||
            reservation.value("scale").toDouble() != 1.0 ||
            client.value("monitor") != QStringLiteral("DP-2") ||
            client.value("is_floating") != false || client.value("is_fullscreen") != false ||
            width <= 0 || height <= 0 || width > bw - 640 || height > bh ||
            x != bx + bw - width || y != by + (bh - height) / 2) {
        return invalid();
    }
    if (QSize(requestedWidth, requestedHeight) != size || QSize(width, height) != size) {
        // The query may overtake the surface commit containing the new size hints.
        return {ReservationState::Pending, {}};
    }
    return {ReservationState::Accepted, {}};
}

MangoNativeSplit::MangoNativeSplit(SDL_Window* window, QSize size) : m_Window(window)
{
    SDL_SetWindowData(window, WindowData, this);
    setSize(size);
}

MangoNativeSplit::~MangoNativeSplit()
{
    stopQuery();
    SDL_SetWindowData(m_Window, WindowData, nullptr);
}

QSize MangoNativeSplit::size() const
{
    const quint64 packed = m_Size.load();
    return QSize(int(packed >> 32), int(packed & 0xffffffff));
}

void MangoNativeSplit::setSize(QSize size)
{
    block();
    m_Size.store((quint64(quint32(size.width())) << 32) | quint32(size.height()));
    SDL_SetWindowMaximumSize(m_Window, 32767, 32767);
    SDL_SetWindowMinimumSize(m_Window, size.width(), size.height());
    SDL_SetWindowMaximumSize(m_Window, size.width(), size.height());
    SDL_SetWindowSize(m_Window, size.width(), size.height());
}

void MangoNativeSplit::block()
{
    m_Allowed.store(false);
}

void MangoNativeSplit::stopQuery()
{
    if (m_Query.state() != QProcess::NotRunning) {
        m_Query.kill();
        m_Query.waitForFinished(QueryTimeoutMs);
    }
    m_QueryStarted = -1;
}

void MangoNativeSplit::begin()
{
    block();
    stopQuery();
    m_Deadline.start();
    m_NextQuery = 0;
}

QString MangoNativeSplit::poll()
{
    if (!waiting()) return {};
    const auto elapsed = m_Deadline.elapsed();
    if (elapsed >= DeadlineMs) {
        stopQuery();
        m_Deadline.invalidate();
        return tr("Timed out waiting for Mango to reserve a pixel-exact stream region. Check that the matching patched Mango package is running.");
    }

    if (m_QueryStarted < 0) {
        if (elapsed < m_NextQuery) return {};
        const auto executable = QStandardPaths::findExecutable("mmsg");
        if (executable.isEmpty()) return tr("The Mango native split requires mmsg on PATH.");
        m_Query.start(executable, {QStringLiteral("get"), QStringLiteral("all-clients")});
        m_QueryStarted = elapsed;
    }

    // The session uses SDL's event loop, not Qt's. Zero-time waits drain QProcess
    // without blocking input or requiring a second event-loop thread.
    if (!m_Query.waitForFinished(0) && m_Query.state() != QProcess::NotRunning) {
        if (elapsed - m_QueryStarted >= QueryTimeoutMs) {
            stopQuery();
            return tr("Mango's window query did not respond. The native split could not be verified.");
        }
        return {};
    }
    m_QueryStarted = -1;
    if (m_Query.error() == QProcess::FailedToStart || m_Query.exitStatus() != QProcess::NormalExit || m_Query.exitCode() != 0) {
        return tr("Unable to query Mango's native split. Check that the matching patched Mango package is running.");
    }
    const auto json = m_Query.readAllStandardOutput();
    m_Query.readAllStandardError();
    if (json.size() > 1024 * 1024) return tr("Mango returned an invalid window query response.");
    const auto reply = parseReply(json, QCoreApplication::applicationPid(), size());
    if (reply.status == ReservationState::Rejected) return reply.error;
    if (reply.status == ReservationState::Accepted && drawableSize(m_Window) == size()) {
        m_Deadline.invalidate();
        m_Allowed.store(true);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Mango native split accepted: %dx%d physical pixels on DP-2", size().width(), size().height());
    }
    else {
        m_NextQuery = elapsed + 50;
    }
    return {};
}

QString MangoNativeSplit::rejectionOnClose()
{
    stopQuery();
    const auto executable = QStandardPaths::findExecutable("mmsg");
    if (executable.isEmpty()) return {};
    m_Query.start(executable, {QStringLiteral("get"), QStringLiteral("all-clients")});
    if (!m_Query.waitForFinished(QueryTimeoutMs)) return {};
    const auto reply = parseReply(m_Query.readAllStandardOutput(), QCoreApplication::applicationPid(), size());
    return reply.status == ReservationState::Rejected ? reply.error : QString();
}

bool MangoNativeSplit::canPresentFrame(int width, int height)
{
    if (!accepted()) return false;
    if (QSize(width, height) == size() && drawableSize(m_Window) == size()) return true;

    if (m_Allowed.exchange(false)) {
        SDL_Event event = {};
        event.type = SDL_USEREVENT;
        event.user.code = RecheckEvent;
        event.user.data1 = reinterpret_cast<void*>(intptr_t(width));
        event.user.data2 = reinterpret_cast<void*>(intptr_t(height));
        SDL_PushEvent(&event);
    }
    return false;
}
