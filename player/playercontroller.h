#ifndef PLAYERCONTROLLER_H
#define PLAYERCONTROLLER_H

#include <QImage>
#include <QObject>
#include <QString>

class VideoDecodeThread;

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
    void teardownThread();

    QString m_filePath;
    VideoDecodeThread *m_thread = nullptr;
    bool m_playing = false;
};

#endif // PLAYERCONTROLLER_H
