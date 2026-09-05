#pragma once

#include "SDL_compat.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QSize>
#include <atomic>

// Owned by the SDL session thread. Only the presentation gate is used by Pacer.
class MangoNativeSplit
{
    Q_DECLARE_TR_FUNCTIONS(MangoNativeSplit)
public:
    static constexpr int RecheckEvent = 106;
    static constexpr const char* WindowTitle = "Linuxmis Native Stream";

    enum class ReservationState { Pending, Accepted, Rejected };
    struct Reply {
        ReservationState status;
        QString error;
    };

    static bool isEnabled();
    static MangoNativeSplit* fromWindow(SDL_Window* window);
    static Reply parseReply(const QByteArray& json, qint64 pid, QSize size);

    MangoNativeSplit(SDL_Window* window, QSize size);
    ~MangoNativeSplit();
    void setSize(QSize size);
    QSize size() const;
    void block();
    void begin();
    QString poll();
    QString rejectionOnClose();
    bool waiting() const { return m_Deadline.isValid(); }
    bool accepted() const { return m_Allowed.load(); }
    bool canPresentFrame(int width, int height);

private:
    void stopQuery();
    SDL_Window* m_Window;
    QProcess m_Query;
    QElapsedTimer m_Deadline;
    qint64 m_QueryStarted = -1;
    qint64 m_NextQuery = 0;
    std::atomic<quint64> m_Size{0};
    std::atomic<bool> m_Allowed{false};
};
