#ifndef VIDEODECODERTHREAD_H
#define VIDEODECODERTHREAD_H

#include <QImage>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>

#include <atomic>

#include "videostreaminfo.h"

struct SwsContext;

class PacketQueue;

class VideoDecoderThread : public QThread
{
    Q_OBJECT

public:
    explicit VideoDecoderThread(QObject *parent = nullptr);
    ~VideoDecoderThread() override;

    void setPacketQueue(PacketQueue *packetQueue);
    void setStreamInfo(const VideoStreamInfo &streamInfo);
    void setSpeed(double speed);
    void setPaused(bool paused);
    void requestStop();

signals:
    void frameReady(const QImage &image, double ptsSeconds);
    void playbackTimeChanged(double ptsSeconds);
    void finishedWithError(const QString &message);

protected:
    void run() override;

private:
    bool shouldStop() const;
    void waitWhilePaused();

    PacketQueue *m_packetQueue = nullptr;
    VideoStreamInfo m_streamInfo;

    mutable QMutex m_stateMutex;
    QWaitCondition m_stateChanged;

    std::atomic_bool m_stopRequested{false};
    bool m_paused = false;
    double m_speed = 1.0;
};

#endif // VIDEODECODERTHREAD_H
