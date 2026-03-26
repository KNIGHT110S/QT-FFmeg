#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMargins>
#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QKeyEvent;
class PlayerController;
class VideoRenderWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;

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
    void toggleFullscreen();
    void setFullscreenEnabled(bool enabled);
    static QString formatTimestamp(double seconds);

    Ui::MainWindow *ui;
    PlayerController *m_player = nullptr;
    VideoRenderWidget *m_videoWidget = nullptr;
    double m_durationSeconds = 0.0;
    bool m_isFullscreen = false;
    bool m_restoreMaximizedAfterFullscreen = false;
    QMargins m_normalCentralMargins;
    int m_normalCentralSpacing = 0;
    QMargins m_normalVideoMargins;
};
#endif // MAINWINDOW_H
