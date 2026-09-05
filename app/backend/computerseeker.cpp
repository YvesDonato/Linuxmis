#include "computerseeker.h"
#include "computermanager.h"
#include <QTimer>
#include <QDebug>

ComputerSeeker::ComputerSeeker(ComputerManager *manager, QString computerName, QObject *parent)
    : QObject(parent), m_ComputerManager(manager), m_ComputerName(computerName),
      m_TimeoutTimer(new QTimer(this))
{
    m_TimeoutTimer->setSingleShot(true);
    connect(m_TimeoutTimer, &QTimer::timeout,
            this, &ComputerSeeker::onTimeout);
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &ComputerSeeker::onComputerUpdated);
}

ComputerSeeker::~ComputerSeeker()
{
    stop();
}

void ComputerSeeker::stop()
{
    m_TimeoutTimer->stop();
    if (m_Polling && m_ComputerManager) {
        m_ComputerManager->stopPollingAsync();
    }
    m_Polling = false;
}

void ComputerSeeker::start(int timeout)
{
    if (m_Polling || !m_ComputerManager) {
        return;
    }

    m_Elapsed.start();
    m_TimeoutTimer->start(timeout);
    m_Polling = true;
    m_ComputerManager->startPolling();

    bool knownHost = false;
    for (NvComputer* computer : m_ComputerManager->getComputers()) {
        if (matchComputer(computer)) {
            knownHost = true;
            if (!isOnline(computer)) {
                computer->wake();
            }
        }
    }

    // Defer notification so all launchers can finish installing their listeners.
    QTimer::singleShot(0, this, [this]() {
        if (m_ComputerManager) {
            for (NvComputer* computer : m_ComputerManager->getComputers()) {
                onComputerUpdated(computer);
            }
        }
    });

    // Seek desired computer by both connecting to it directly (this may fail
    // if m_ComputerName is UUID, or the name that doesn't resolve to an IP
    // address) and by polling it using mDNS, hopefully one of these methods
    // would find the host
    if (!knownHost) {
        m_ComputerManager->addNewHostManually(m_ComputerName);
    }
}

void ComputerSeeker::onComputerUpdated(NvComputer *computer)
{
    if (!m_TimeoutTimer->isActive()) {
        return;
    }
    if (matchComputer(computer) && isOnline(computer)) {
        QString endpoint;
        {
            QReadLocker lock(&computer->lock);
            endpoint = computer->activeAddress.toString();
        }
        stop();
        qInfo() << "Host discovery completed in" << m_Elapsed.elapsed() << "ms at"
                << endpoint;
        emit computerFound(computer);
    }
}

bool ComputerSeeker::matchComputer(NvComputer *computer) const
{
    QString value = m_ComputerName.toLower();

    {
        QReadLocker lock(&computer->lock);
        if (computer->name.toLower() == value || computer->uuid.toLower() == value) {
            return true;
        }
    }

    for (const NvAddress& addr : computer->uniqueAddresses()) {
        if (addr.address().toLower() == value || addr.toString().toLower() == value) {
            return true;
        }
    }

    return false;
}

bool ComputerSeeker::isOnline(NvComputer *computer) const
{
    QReadLocker lock(&computer->lock);
    return computer->state == NvComputer::CS_ONLINE;
}

void ComputerSeeker::onTimeout()
{
    stop();
    qWarning() << "Host discovery timed out after" << m_Elapsed.elapsed() << "ms for" << m_ComputerName;
    emit errorTimeout();
}
