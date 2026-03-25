#ifndef VIDEODECODETHREAD_H
#define VIDEODECODETHREAD_H

#include <QImage>
#include <QMutex>
#include <QString>
#include <QThread>
#include <QWaitCondition>

#include <atomic>

struct AVCodecContext;
struct AVFormatContext;
struct AVStream;
struct SwsContext;

class VideoDecodeThread : public QThread
{
    Q_OBJECT

public:
    explicit VideoDecodeThread(QObject *parent = nullptr);
    ~VideoDecodeThread() override;

    void setFilePath(const QString &filePath);
    void setSpeed(double speed);
    void setPaused(bool paused);
    void requestStop();

signals:
    void opened(double durationSeconds, int width, int height, double fps);
    void frameReady(const QImage &image, double ptsSeconds);
    void playbackTimeChanged(double ptsSeconds);
    void finishedWithError(const QString &message);
    void stopped();

protected:
    void run() override;

private:
    bool shouldStop() const;
    void waitWhilePaused();

    QString m_filePath;

    mutable QMutex m_stateMutex;
    QWaitCondition m_stateChanged;

    std::atomic_bool m_stopRequested{false};
    bool m_paused = false;
    double m_speed = 1.0;
};

#endif // VIDEODECODETHREAD_H
