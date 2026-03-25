#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "player/playercontroller.h"
#include "player/videorenderwidget.h"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMessageBox>

namespace {

static double parseSpeedText(const QString &text)
{
    QString s = text;
    s.remove(QChar(u'x'), Qt::CaseInsensitive);
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok && v > 0.0 ? v : 1.0;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_player = new PlayerController(this);

    setWindowTitle("QT FFmpeg Player");
    ui->speedComboBox->setCurrentText("1.0x");
    ui->playerControlsLayout->setColumnStretch(0, 0);
    ui->playerControlsLayout->setColumnStretch(1, 1);
    ui->playerControlsLayout->setColumnStretch(2, 0);

    m_videoWidget = new VideoRenderWidget(ui->videoContainer);
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (ui->videoLayout->count() > 0) {
        QLayoutItem *item = ui->videoLayout->takeAt(0);
        delete item; // remove the placeholder spacer
    }
    ui->videoLayout->addWidget(m_videoWidget);

    ui->currentTimeLabel->setText("00:00");
    ui->durationLabel->setText("00:00");
    ui->progressSlider->setValue(0);
    ui->volumeValueLabel->setText(QString("%1%").arg(ui->volumeSlider->value()));
    ui->playPauseButton->setText("Play");

    auto *openAction = new QAction(this);
    openAction->setShortcut(QKeySequence::Open);
    addAction(openAction);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenFileTriggered);

    connect(ui->openFileButton, &QPushButton::clicked, this, &MainWindow::onOpenFileTriggered);
    connect(ui->playPauseButton, &QPushButton::clicked, this, &MainWindow::onPlayPauseClicked);
    connect(ui->speedComboBox, &QComboBox::currentTextChanged, this, &MainWindow::onSpeedChanged);
    connect(ui->volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);

    connect(m_player, &PlayerController::opened, this, &MainWindow::onOpened);
    connect(m_player, &PlayerController::frameReady, m_videoWidget, &VideoRenderWidget::setFrame);
    connect(m_player, &PlayerController::playbackTimeChanged, this, &MainWindow::onPlaybackTimeChanged);
    connect(m_player, &PlayerController::errorOccurred, this, &MainWindow::onError);
    connect(m_player, &PlayerController::stopped, this, &MainWindow::onStopped);

    setStyleSheet(
        "QMainWindow {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "        stop:0 #11161c, stop:0.55 #18212a, stop:1 #0d1117);"
        "}"
        "QWidget#centralwidget {"
        "    background: transparent;"
        "}"
        "QFrame#videoContainer {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "        stop:0 #0f1318, stop:0.5 #141b23, stop:1 #0a0e13);"
        "    border: 1px solid #243240;"
        "    border-radius: 26px;"
        "}"
        "QFrame#controlsPanel {"
        "    background: rgba(12, 18, 24, 0.9);"
        "    border: 1px solid #263340;"
        "    border-radius: 22px;"
        "}"
        "QPushButton#playPauseButton {"
        "    background: #d9a441;"
        "    color: #11161c;"
        "    border: none;"
        "    border-radius: 14px;"
        "    padding: 7px 16px;"
        "}"
        "QPushButton#playPauseButton:hover {"
        "    background: #e5b45b;"
        "}"
        "QPushButton#playPauseButton:pressed {"
        "    background: #c58d2a;"
        "}"
        "QPushButton#openFileButton {"
        "    background: #1b2530;"
        "    color: #f4f4ef;"
        "    border: 1px solid #304252;"
        "    border-radius: 14px;"
        "    padding: 7px 16px;"
        "}"
        "QPushButton#openFileButton:hover {"
        "    background: #223142;"
        "}"
        "QPushButton#openFileButton:pressed {"
        "    background: #15202c;"
        "}"
        "QSlider::groove:horizontal {"
        "    border: none;"
        "    height: 6px;"
        "    background: #28323c;"
        "    border-radius: 3px;"
        "}"
        "QSlider#progressSlider::sub-page:horizontal {"
        "    background: #d9a441;"
        "    border-radius: 3px;"
        "}"
        "QSlider#volumeSlider::sub-page:horizontal {"
        "    background: #78a9ff;"
        "    border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal {"
        "    background: #f4f4ef;"
        "    width: 16px;"
        "    margin: -6px 0;"
        "    border-radius: 8px;"
        "}"
        "QComboBox#speedComboBox {"
        "    background: #1b2530;"
        "    color: #f4f4ef;"
        "    border: 1px solid #304252;"
        "    border-radius: 10px;"
        "    padding: 6px 10px;"
        "}"
        "QComboBox#speedComboBox::drop-down {"
        "    border: none;"
        "    width: 24px;"
        "}"
        "QComboBox#speedComboBox QAbstractItemView {"
        "    background: #141c24;"
        "    color: #f4f4ef;"
        "    border: 1px solid #304252;"
        "    selection-background-color: #273747;"
        "}"
    );
}

MainWindow::~MainWindow()
{
    if (m_player) {
        m_player->stop();
    }
    delete ui;
}

void MainWindow::onPlayPauseClicked()
{
    if (!m_player->hasFile()) {
        onOpenFileTriggered();
        return;
    }

    if (m_player->isPlaying()) {
        m_player->pause();
        ui->playPauseButton->setText("Resume");
    } else {
        m_player->play();
        ui->playPauseButton->setText("Pause");
    }
}

void MainWindow::onOpenFileTriggered()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Open Video",
        QString(),
        "Video Files (*.mp4 *.mkv *.mov *.avi *.flv *.wmv *.m4v);;All Files (*.*)");

    if (filePath.isEmpty()) {
        return;
    }

    m_durationSeconds = 0.0;
    ui->currentTimeLabel->setText("00:00");
    ui->durationLabel->setText("00:00");
    ui->progressSlider->setValue(0);
    if (m_videoWidget) {
        m_videoWidget->clearFrame();
    }

    const QFileInfo fi(filePath);
    setWindowTitle(QString("QT FFmpeg Player  |  %1").arg(fi.fileName()));

    m_player->openFile(filePath, true);
    m_player->setSpeed(parseSpeedText(ui->speedComboBox->currentText()));
    ui->playPauseButton->setText("Pause");
}

void MainWindow::onSpeedChanged(const QString &text)
{
    if (!m_player) {
        return;
    }
    m_player->setSpeed(parseSpeedText(text));
}

void MainWindow::onVolumeChanged(int value)
{
    ui->volumeValueLabel->setText(QString("%1%").arg(value));
    // Phase B: video-only, volume will be wired when audio output is implemented.
}

void MainWindow::onOpened(double durationSeconds, int width, int height, double fps)
{
    m_durationSeconds = durationSeconds;
    ui->durationLabel->setText(formatTimestamp(durationSeconds));
    ui->progressSlider->setValue(0);
    ui->playPauseButton->setText("Pause");
    setWindowTitle(QString("QT FFmpeg Player  |  %1x%2  %3 fps")
                       .arg(width)
                       .arg(height)
                       .arg(fps > 0.0 ? fps : 0.0, 0, 'f', 2));
}

void MainWindow::onPlaybackTimeChanged(double ptsSeconds)
{
    ui->currentTimeLabel->setText(formatTimestamp(ptsSeconds));

    if (m_durationSeconds > 1e-6) {
        const double ratio = qBound(0.0, ptsSeconds / m_durationSeconds, 1.0);
        ui->progressSlider->setValue(static_cast<int>(ratio * ui->progressSlider->maximum()));
    }
}

void MainWindow::onError(const QString &message)
{
    ui->playPauseButton->setText("Play");
    setWindowTitle(QString("QT FFmpeg Player  |  Error: %1").arg(message));
    QMessageBox::warning(this, "Playback Error", message);
}

void MainWindow::onStopped()
{
    if (!m_player->hasFile()) {
        ui->playPauseButton->setText("Play");
        return;
    }
    ui->playPauseButton->setText("Play");
}

QString MainWindow::formatTimestamp(double seconds)
{
    if (seconds < 0.0) {
        seconds = 0.0;
    }

    const int total = static_cast<int>(seconds + 0.5);
    const int hh = total / 3600;
    const int mm = (total % 3600) / 60;
    const int ss = total % 60;

    if (hh > 0) {
        return QString("%1:%2:%3")
            .arg(hh, 2, 10, QLatin1Char('0'))
            .arg(mm, 2, 10, QLatin1Char('0'))
            .arg(ss, 2, 10, QLatin1Char('0'));
    }

    return QString("%1:%2")
        .arg(mm, 2, 10, QLatin1Char('0'))
        .arg(ss, 2, 10, QLatin1Char('0'));
}
