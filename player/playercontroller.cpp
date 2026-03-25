#include "playercontroller.h"

#include "videodecodethread.h"

#include <QPointer>

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
{
}

PlayerController::~PlayerController()
{
    stop();
}

bool PlayerController::openFile(const QString &filePath, bool autoPlay)
{
    if (filePath.isEmpty()) {
        return false;
    }

    stop();
    m_filePath = filePath;

    m_thread = new VideoDecodeThread(this);
    m_thread->setFilePath(filePath);
    m_thread->setPaused(!autoPlay);

    VideoDecodeThread *const thread = m_thread;
    const QPointer<VideoDecodeThread> guardedThread(thread);

    connect(thread, &VideoDecodeThread::opened, this, &PlayerController::opened);
    connect(thread, &VideoDecodeThread::frameReady, this, &PlayerController::frameReady);
    connect(thread, &VideoDecodeThread::playbackTimeChanged, this, &PlayerController::playbackTimeChanged);
    connect(thread, &VideoDecodeThread::finishedWithError, this, [this](const QString &message) {
        m_playing = false;
        emit errorOccurred(message);
    });
    connect(thread, &VideoDecodeThread::stopped, this, [this, guardedThread]() {
        m_playing = false;
        if (m_thread == guardedThread.data()) {
            m_thread = nullptr;
        }
        emit stopped();
    });
    connect(thread, &QThread::finished, this, [this, guardedThread]() {
        if (m_thread == guardedThread.data()) {
            m_thread = nullptr;
        }
        if (guardedThread) {
            guardedThread->deleteLater();
        }
    });

    m_playing = autoPlay;
    thread->start();
    return true;
}

void PlayerController::play()
{
    if (!m_thread) {
        if (!m_filePath.isEmpty()) {
            const QString filePath = m_filePath;
            openFile(filePath, true);
        }
        return;
    }
    m_playing = true;
    m_thread->setPaused(false);
}

void PlayerController::pause()
{
    if (!m_thread) {
        return;
    }
    m_playing = false;
    m_thread->setPaused(true);
}

void PlayerController::stop()
{
    teardownThread();
    m_playing = false;
    m_filePath.clear();
}

void PlayerController::setSpeed(double speed)
{
    if (m_thread) {
        m_thread->setSpeed(speed);
    }
}

bool PlayerController::hasFile() const
{
    return !m_filePath.isEmpty();
}

bool PlayerController::isPlaying() const
{
    return m_playing;
}

void PlayerController::teardownThread()
{
    if (!m_thread) {
        return;
    }

    m_thread->requestStop();
    m_thread->wait(2000);
    delete m_thread;
    m_thread = nullptr;
}
