#ifndef PACKETQUEUE_H
#define PACKETQUEUE_H

#include <QMutex>
#include <QWaitCondition>

#include <deque>

struct AVPacket;

class PacketQueue
{
public:
    explicit PacketQueue(int maxPackets = 96);
    ~PacketQueue();

    bool push(const AVPacket *packet);
    bool pop(AVPacket *packet);

    void setFinished();
    void abort();
    void flush();

    bool isFinished() const;

private:
    mutable QMutex m_mutex;
    QWaitCondition m_notEmpty;
    QWaitCondition m_notFull;
    std::deque<AVPacket *> m_packets;
    int m_maxPackets = 0;
    bool m_finished = false;
    bool m_aborted = false;
};

#endif // PACKETQUEUE_H
