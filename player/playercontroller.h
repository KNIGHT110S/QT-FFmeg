#ifndef PLAYERCONTROLLER_H
#define PLAYERCONTROLLER_H

#include <QImage>
#include <QObject>
#include <QString>

class DemuxThread;
class PacketQueue;
class VideoDecoderThread;

class PlayerController : public QObject
{
    Q_OBJECT

public:
    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController() override;

    bool openFile(const QString &filePath, bool autoPlay = true);
    void play();
    void pause();
    void stop();
    void setSpeed(double speed);

    bool hasFile() const;
    bool isPlaying() const;

signals:
    void opened(double durationSeconds, int width, int height, double fps);
    void frameReady(const QImage &image, double ptsSeconds);
    void playbackTimeChanged(double ptsSeconds);
    void errorOccurred(const QString &message);
    void stopped();

private:
    void handlePipelineError(const QString &message);
    void notifyStoppedIfIdle();
    void teardownPipeline();

    QString m_filePath;
    DemuxThread *m_demuxThread = nullptr;
    VideoDecoderThread *m_decodeThread = nullptr;
    PacketQueue *m_packetQueue = nullptr;
    bool m_playing = false;
    bool m_stopNotified = true;
    bool m_errorNotified = false;
    bool m_decoderLaunchAttempted = false;
    double m_speed = 1.0;
};

#endif // PLAYERCONTROLLER_H
