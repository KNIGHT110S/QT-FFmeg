#include "playercontroller.h"

#include "demuxthread.h"
#include "packetqueue.h"
#include "videodecoderthread.h"
#include "videostreaminfo.h"

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<VideoStreamInfo>();
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
    m_playing = autoPlay;
    m_stopNotified = false;
    m_errorNotified = false;
    m_decoderLaunchAttempted = false;

    m_packetQueue = new PacketQueue();
    m_demuxThread = new DemuxThread(this);
    m_demuxThread->setFilePath(filePath);
    m_demuxThread->setPacketQueue(m_packetQueue);
    DemuxThread *const demuxThread = m_demuxThread;

    connect(demuxThread, &DemuxThread::streamInfoReady, this, [this, demuxThread](const VideoStreamInfo &streamInfo) {
        if (m_demuxThread != demuxThread || !m_packetQueue) {
            return;
        }

        m_decoderLaunchAttempted = true;
        emit opened(streamInfo.durationSeconds, streamInfo.width, streamInfo.height, streamInfo.fps);

        if (m_decodeThread) {
            return;
        }

        m_decodeThread = new VideoDecoderThread(this);
        m_decodeThread->setPacketQueue(m_packetQueue);
        m_decodeThread->setStreamInfo(streamInfo);
        m_decodeThread->setPaused(!m_playing);
        m_decodeThread->setSpeed(m_speed);
        VideoDecoderThread *const decodeThread = m_decodeThread;

        connect(decodeThread, &VideoDecoderThread::frameReady, this, [this, decodeThread](const QImage &image, double ptsSeconds) {
            if (m_decodeThread != decodeThread) {
                return;
            }
            emit frameReady(image, ptsSeconds);
        });
        connect(decodeThread, &VideoDecoderThread::playbackTimeChanged, this, [this, decodeThread](double ptsSeconds) {
            if (m_decodeThread != decodeThread) {
                return;
            }
            emit playbackTimeChanged(ptsSeconds);
        });
        connect(decodeThread, &VideoDecoderThread::finishedWithError, this, [this, decodeThread](const QString &message) {
            if (m_decodeThread != decodeThread) {
                return;
            }
            handlePipelineError(message);
        });
        connect(decodeThread, &QThread::finished, this, [this, decodeThread]() {
            if (m_decodeThread != decodeThread) {
                return;
            }
            m_playing = false;
            notifyStoppedIfIdle();
        });

        decodeThread->start();
    });
    connect(demuxThread, &DemuxThread::finishedWithError, this, [this, demuxThread](const QString &message) {
        if (m_demuxThread != demuxThread) {
            return;
        }
        handlePipelineError(message);
    });
    connect(demuxThread, &QThread::finished, this, [this, demuxThread]() {
        if (m_demuxThread != demuxThread) {
            return;
        }
        notifyStoppedIfIdle();
    });

    demuxThread->start();
    return true;
}

void PlayerController::play()
{
    if (m_filePath.isEmpty()) {
        return;
    }

    if ((!m_demuxThread || !m_demuxThread->isRunning())
        && (!m_decodeThread || !m_decodeThread->isRunning())) {
        const QString filePath = m_filePath;
        openFile(filePath, true);
        return;
    }

    m_playing = true;
    if (m_decodeThread) {
        m_decodeThread->setPaused(false);
    }
}

void PlayerController::pause()
{
    m_playing = false;
    if (m_decodeThread) {
        m_decodeThread->setPaused(true);
    }
}

void PlayerController::stop()
{
    teardownPipeline();
    m_playing = false;
    m_filePath.clear();
}

void PlayerController::setSpeed(double speed)
{
    if (speed <= 0.0) {
        speed = 1.0;
    }

    m_speed = speed;
    if (m_decodeThread) {
        m_decodeThread->setSpeed(speed);
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

void PlayerController::handlePipelineError(const QString &message)
{
    m_playing = false;

    if (!m_errorNotified) {
        m_errorNotified = true;
        emit errorOccurred(message);
    }

    if (m_demuxThread && m_demuxThread->isRunning()) {
        m_demuxThread->requestStop();
    }
    if (m_decodeThread && m_decodeThread->isRunning()) {
        m_decodeThread->requestStop();
    }
    if (m_packetQueue) {
        m_packetQueue->abort();
    }
}

void PlayerController::notifyStoppedIfIdle()
{
    if (m_stopNotified) {
        return;
    }

    const bool demuxIdle = !m_demuxThread || !m_demuxThread->isRunning();
    const bool decodeIdle = !m_decodeThread || !m_decodeThread->isRunning();
    if (!demuxIdle || !decodeIdle) {
        return;
    }
    if (!m_decodeThread && !m_decoderLaunchAttempted && !m_errorNotified) {
        return;
    }

    m_playing = false;
    m_stopNotified = true;
    emit stopped();
}

void PlayerController::teardownPipeline()
{
    if (m_demuxThread) {
        m_demuxThread->requestStop();
    }
    if (m_decodeThread) {
        m_decodeThread->requestStop();
    }
    if (m_packetQueue) {
        m_packetQueue->abort();
    }

    if (m_demuxThread) {
        m_demuxThread->wait(2000);
        delete m_demuxThread;
        m_demuxThread = nullptr;
    }
    if (m_decodeThread) {
        m_decodeThread->wait(2000);
        delete m_decodeThread;
        m_decodeThread = nullptr;
    }
    if (m_packetQueue) {
        delete m_packetQueue;
        m_packetQueue = nullptr;
    }

    m_stopNotified = true;
    m_errorNotified = false;
    m_decoderLaunchAttempted = false;
}
