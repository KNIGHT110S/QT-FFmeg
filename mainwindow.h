#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class PlayerController;
class VideoRenderWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onPlayPauseClicked();
    void onOpenFileTriggered();
    void onSpeedChanged(const QString &text);
    void onVolumeChanged(int value);

    void onOpened(double durationSeconds, int width, int height, double fps);
    void onPlaybackTimeChanged(double ptsSeconds);
    void onError(const QString &message);
    void onStopped();

private:
    static QString formatTimestamp(double seconds);

    Ui::MainWindow *ui;
    PlayerController *m_player = nullptr;
    VideoRenderWidget *m_videoWidget = nullptr;
    double m_durationSeconds = 0.0;
};
#endif // MAINWINDOW_H
