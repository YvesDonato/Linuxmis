#include "startstream.h"
#include "backend/computermanager.h"
#include "backend/computerseeker.h"
#include "backend/nvhttp.h"
#include "streaming/session.h"

#include <QCoreApplication>
#include <QTimer>
#include <QPointer>

#define COMPUTER_SEEK_TIMEOUT 30000
#define APP_QUIT_TIMEOUT 30000

namespace CliStartStream
{

enum State {
    StateInit,
    StateSeekComputer,
    StateSeekApp,
    StateAwaitQuit,
    StateQuitting,
    StateStartSession,
    StateFailure,
};

class Event
{
public:
    enum Type {
        AppQuitCompleted,
        AppQuitRequested,
        ComputerFound,
        ComputerUpdated,
        Executed,
        Timedout,
    };

    Event(Type type)
        : type(type), computerManager(nullptr), computer(nullptr) {}

    Type type;
    ComputerManager *computerManager;
    NvComputer *computer;
    QString errorMessage;
};

class LauncherPrivate
{
    Q_DECLARE_PUBLIC(Launcher)

public:
    LauncherPrivate(Launcher *q) : q_ptr(q) {}

    void handleEvent(Event event)
    {
        Q_Q(Launcher);
        switch (event.type) {
        // Occurs when CliStartStreamSegue becomes visible and the UI calls launcher's execute()
        case Event::Executed:
            if (m_State == StateInit) {
                m_State = StateSeekComputer;
                m_ComputerManager = event.computerManager;

                m_ComputerSeeker = new ComputerSeeker(m_ComputerManager, m_ComputerName, q);
                q->connect(m_ComputerSeeker, &ComputerSeeker::computerFound,
                           q, &Launcher::onComputerFound);
                q->connect(m_ComputerSeeker, &ComputerSeeker::errorTimeout,
                           q, &Launcher::onTimeout);
                q->connect(m_ComputerManager, &ComputerManager::computerStateChanged,
                           q, &Launcher::onComputerUpdated);
                q->connect(m_ComputerManager, &ComputerManager::quitAppCompleted,
                           q, &Launcher::onQuitAppCompleted);

                emit q->searchingComputer();
                m_ComputerSeeker->start(COMPUTER_SEEK_TIMEOUT);
            }
            break;
        // Occurs when searched computer is found
        case Event::ComputerFound:
            if (m_State == StateSeekComputer) {
                if (event.computer->pairState == NvComputer::PS_PAIRED) {
                    m_State = StateSeekApp;
                    m_Computer = event.computer;
                    emit q->searchingApp();

                    // Polling may emit no further change if the cached list is unchanged.
                    // Fetch explicitly, as the CLI list command does.
                    try {
                        NvHTTP http(m_Computer);
                        m_Apps = http.getAppList();
                        startSessionOrRequestQuit();
                    } catch (const std::exception& exception) {
                        m_State = StateFailure;
                        emit q->failed(QString::fromUtf8(exception.what()));
                    }
                } else {
                    m_State = StateFailure;
                    QString msg = QObject::tr("Computer %1 has not been paired. "
                                              "Please open Moonlight to pair before streaming.")
                            .arg(event.computer->name);
                    emit q->failed(msg);
                }
            }
            break;
        // Occurs when a computer is updated
        case Event::ComputerUpdated:
            if (m_State == StateQuitting && event.computer == m_Computer) {
                bool stopped;
                {
                    QReadLocker lock(&m_Computer->lock);
                    stopped = m_Computer->state == NvComputer::CS_ONLINE && isNotStreaming();
                }
                if (stopped) {
                    startSessionOrRequestQuit();
                }
            }
            break;
        // Occurs when there was another app running on computer and user accepted quit
        // confirmation dialog
        case Event::AppQuitRequested:
            if (m_State == StateAwaitQuit) {
                {
                    QReadLocker lock(&m_Computer->lock);
                    if (isNotStreaming()) {
                        lock.unlock();
                        startSessionOrRequestQuit();
                        break;
                    }
                }
                m_State = StateQuitting;
                m_TimeoutTimer->start(APP_QUIT_TIMEOUT);
                m_ComputerManager->quitRunningApp(m_Computer);
            }
            break;
        // Occurs when the previous app quit has been completed, handles quitting errors if any
        // happened. ComputerUpdated event's handler handles session start when previous app has
        // quit.
        case Event::AppQuitCompleted:
            if (m_State == StateQuitting && !event.errorMessage.isEmpty()) {
                m_State = StateFailure;
                stopPolling();
                emit q->failed(QObject::tr("Quitting app failed, reason: %1").arg(event.errorMessage));
            }
            break;
        // Occurs when computer or app search timed out
        case Event::Timedout:
            if (m_State == StateSeekComputer) {
                m_State = StateFailure;
                emit q->failed(QObject::tr("Failed to connect to %1").arg(m_ComputerName));
            }
            if (m_State == StateQuitting) {
                m_State = StateFailure;
                stopPolling();
                emit q->failed(QObject::tr("Quitting app failed, reason: %1").arg(QStringLiteral("Request timed out")));
            }
            break;
        }
    }

    void stopPolling()
    {
        m_TimeoutTimer->stop();
        if (m_Polling && m_ComputerManager) {
            m_ComputerManager->stopPollingAsync();
        }
        m_Polling = false;
    }

    void startSessionOrRequestQuit()
    {
        Q_Q(Launcher);
        NvApp app;
        QString currentAppName;
        bool canStart;
        {
            QReadLocker lock(&m_Computer->lock);
            const int index = getAppIndex();
            if (index == -1) {
                m_State = StateFailure;
                lock.unlock();
                stopPolling();
                emit q->failed(QObject::tr("Failed to find application %1").arg(m_AppName));
                return;
            }
            app = m_Apps[index];
            canStart = isNotStreaming() || isStreamingApp(app);
            currentAppName = getCurrentAppName();
        }

        if (canStart) {
            m_State = StateStartSession;
            stopPolling();
            auto session = new Session(m_Computer, app, m_Preferences);
            emit q->sessionCreated(app.name, session);
        }
        else {
            m_State = StateAwaitQuit;
            if (!m_Polling) {
                m_Polling = true;
                m_ComputerManager->startPolling();
            }
            emit q->appQuitRequired(currentAppName);
        }
    }

    int getAppIndex() const
    {
        for (int i = 0; i < m_Apps.length(); i++) {
            if (m_Apps[i].name.compare(m_AppName, Qt::CaseInsensitive) == 0) {
                return i;
            }
        }
        return -1;
    }

    bool isNotStreaming() const
    {
        return m_Computer->currentGameId == 0;
    }

    bool isStreamingApp(NvApp app) const
    {
        return m_Computer->currentGameId == app.id;
    }

    QString getCurrentAppName() const
    {
        for (const NvApp& app : m_Apps) {
            if (m_Computer->currentGameId == app.id) {
                return app.name;
            }
        }
        return "<UNKNOWN>";
    }

    Launcher *q_ptr;
    QString m_ComputerName;
    QString m_AppName;
    QVector<NvApp> m_Apps;
    StreamingPreferences *m_Preferences;
    QPointer<ComputerManager> m_ComputerManager;
    ComputerSeeker *m_ComputerSeeker;
    NvComputer *m_Computer;
    State m_State;
    QTimer *m_TimeoutTimer;
    bool m_Polling = false;
};

Launcher::Launcher(QString computer, QString app,
                   StreamingPreferences* preferences, QObject *parent)
    : QObject(parent),
      m_DPtr(new LauncherPrivate(this))
{
    Q_D(Launcher);
    d->m_ComputerName = computer;
    d->m_AppName = app;
    d->m_Preferences = preferences;
    d->m_State = StateInit;
    d->m_TimeoutTimer = new QTimer(this);
    d->m_TimeoutTimer->setSingleShot(true);
    connect(d->m_TimeoutTimer, &QTimer::timeout,
            this, &Launcher::onTimeout);
}

Launcher::~Launcher()
{
    Q_D(Launcher);
    d->stopPolling();
}

void Launcher::execute(ComputerManager *manager)
{
    Q_D(Launcher);
    Event event(Event::Executed);
    event.computerManager = manager;
    d->handleEvent(event);
}

void Launcher::quitRunningApp()
{
    Q_D(Launcher);
    Event event(Event::AppQuitRequested);
    d->handleEvent(event);
}

bool Launcher::isExecuted() const
{
    Q_D(const Launcher);
    return d->m_State != StateInit;
}

void Launcher::onComputerFound(NvComputer *computer)
{
    Q_D(Launcher);
    Event event(Event::ComputerFound);
    event.computer = computer;
    d->handleEvent(event);
}

void Launcher::onComputerUpdated(NvComputer *computer)
{
    Q_D(Launcher);
    Event event(Event::ComputerUpdated);
    event.computer = computer;
    d->handleEvent(event);
}

void Launcher::onTimeout()
{
    Q_D(Launcher);
    Event event(Event::Timedout);
    d->handleEvent(event);
}

void Launcher::onQuitAppCompleted(QVariant error)
{
    Q_D(Launcher);
    Event event(Event::AppQuitCompleted);
    event.errorMessage = error.toString();
    d->handleEvent(event);
}

}
