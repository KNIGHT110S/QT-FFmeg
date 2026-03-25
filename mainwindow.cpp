#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("QT FFmpeg Player");
    ui->speedComboBox->setCurrentText("1.0x");
    ui->playerControlsLayout->setColumnStretch(0, 1);
    ui->playerControlsLayout->setColumnStretch(1, 1);
    ui->playerControlsLayout->setColumnStretch(2, 1);
    ui->playerControlsLayout->setAlignment(ui->playPauseButton, Qt::AlignCenter);

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
    delete ui;
}
