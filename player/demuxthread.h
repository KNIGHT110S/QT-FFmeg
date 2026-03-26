#ifndef DEMUXTHREAD_H
#define DEMUXTHREAD_H

#include <QString>
#include <QThread>

#include <atomic>

#include "videostreaminfo.h"

class PacketQueue;

class DemuxThread : public QThread
{
    Q_OBJECT

public:
    explicit DemuxThread(QObject *parent = nullptr);
    ~DemuxThread() override;

    void setFilePath(const QString &filePath);
    void setPacketQueue(PacketQueue *packetQueue);
    void requestStop();

signals:
    void streamInfoReady(const VideoStreamInfo &streamInfo);
    void finishedWithError(const QString &message);

protected:
    void run() override;

private:
    bool shouldStop() const;

    QString m_filePath;
    PacketQueue *m_packetQueue = nullptr;
    std::atomic_bool m_stopRequested{false};
};

#endif // DEMUXTHREAD_H
