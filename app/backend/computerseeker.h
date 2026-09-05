#pragma once

#include <QObject>
#include <QPointer>
#include <QElapsedTimer>

class ComputerManager;
class NvComputer;
class QTimer;

class ComputerSeeker : public QObject
{
    Q_OBJECT
public:
    explicit ComputerSeeker(ComputerManager *manager, QString computerName, QObject *parent = nullptr);
    ~ComputerSeeker() override;

    void start(int timeout);

signals:
    void computerFound(NvComputer *computer);
    void errorTimeout();

private slots:
    void onComputerUpdated(NvComputer *computer);
    void onTimeout();

private:
    void stop();
    bool matchComputer(NvComputer *computer) const;
    bool isOnline(NvComputer *computer) const;

private:
    QPointer<ComputerManager> m_ComputerManager;
    QString m_ComputerName;
    QTimer *m_TimeoutTimer;
    bool m_Polling = false;
    QElapsedTimer m_Elapsed;
};
