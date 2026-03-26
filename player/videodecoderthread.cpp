#include "videodecoderthread.h"

#include "packetqueue.h"

#include <QElapsedTimer>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace {

static QString ffmpegErrToString(int err)
{
    char buf[AV_ERROR_MAX_STRING_SIZE];
    buf[0] = '\0';
    av_strerror(err, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}

} // namespace

VideoDecoderThread::VideoDecoderThread(QObject *parent)
    : QThread(parent)
{
}

VideoDecoderThread::~VideoDecoderThread()
{
    requestStop();
    wait(2000);
}

void VideoDecoderThread::setPacketQueue(PacketQueue *packetQueue)
{
    m_packetQueue = packetQueue;
}

void VideoDecoderThread::setStreamInfo(const VideoStreamInfo &streamInfo)
{
    m_streamInfo = streamInfo;
}

void VideoDecoderThread::setSpeed(double speed)
{
    if (speed <= 0.0) {
        speed = 1.0;
    }

    {
        QMutexLocker locker(&m_stateMutex);
        m_speed = speed;
    }
    m_stateChanged.wakeAll();
}

void VideoDecoderThread::setPaused(bool paused)
{
    {
        QMutexLocker locker(&m_stateMutex);
        m_paused = paused;
    }
    m_stateChanged.wakeAll();
}

void VideoDecoderThread::requestStop()
{
    m_stopRequested.store(true, std::memory_order_relaxed);
    requestInterruption();
    m_stateChanged.wakeAll();
    if (m_packetQueue) {
        m_packetQueue->abort();
    }
}

bool VideoDecoderThread::shouldStop() const
{
    return isInterruptionRequested() || m_stopRequested.load(std::memory_order_relaxed);
}

void VideoDecoderThread::waitWhilePaused()
{
    QMutexLocker locker(&m_stateMutex);
    while (!shouldStop() && m_paused) {
        m_stateChanged.wait(&m_stateMutex, 200);
    }
}

void VideoDecoderThread::run()
{
    if (!m_packetQueue) {
        emit finishedWithError("Packet queue was not initialized.");
        return;
    }

    const VideoStreamInfo streamInfo = m_streamInfo;
    if (!streamInfo.isValid()) {
        emit finishedWithError("Video stream parameters are invalid.");
        return;
    }

    const AVCodecParameters *params = streamInfo.codecParameters();
    const AVCodec *decoder = avcodec_find_decoder(params->codec_id);
    if (!decoder) {
        emit finishedWithError("No suitable video decoder found.");
        return;
    }

    AVCodecContext *codecCtx = avcodec_alloc_context3(decoder);
    AVPacket *packet = nullptr;
    AVFrame *frame = nullptr;
    SwsContext *swsCtx = nullptr;

    if (!codecCtx) {
        emit finishedWithError("avcodec_alloc_context3 failed.");
        return;
    }

    int err = avcodec_parameters_to_context(codecCtx, params);
    if (err < 0) {
        emit finishedWithError(QString("avcodec_parameters_to_context failed: %1").arg(ffmpegErrToString(err)));
        avcodec_free_context(&codecCtx);
        return;
    }

    err = avcodec_open2(codecCtx, decoder, nullptr);
    if (err < 0) {
        emit finishedWithError(QString("avcodec_open2 failed: %1").arg(ffmpegErrToString(err)));
        avcodec_free_context(&codecCtx);
        return;
    }

    packet = av_packet_alloc();
    frame = av_frame_alloc();
    if (!packet || !frame) {
        emit finishedWithError("Failed to allocate AVPacket/AVFrame.");
        av_packet_free(&packet);
        av_frame_free(&frame);
        avcodec_free_context(&codecCtx);
        return;
    }

    bool haveStartPts = false;
    double startPtsSeconds = 0.0;
    QElapsedTimer wallClock;
    wallClock.invalidate();

    qint64 frameIndex = 0;
    const double fallbackFps = streamInfo.fps > 1e-6 ? streamInfo.fps : 25.0;
    int swsSourceWidth = 0;
    int swsSourceHeight = 0;
    AVPixelFormat swsSourceFormat = AV_PIX_FMT_NONE;

    auto processAvailableFrames = [&](bool *fatalError) {
        while (!shouldStop()) {
            waitWhilePaused();
            if (shouldStop()) {
                break;
            }

            err = avcodec_receive_frame(codecCtx, frame);
            if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
                break;
            }
            if (err < 0) {
                emit finishedWithError(QString("avcodec_receive_frame failed: %1").arg(ffmpegErrToString(err)));
                *fatalError = true;
                break;
            }

            qint64 ts = frame->best_effort_timestamp;
            if (ts == AV_NOPTS_VALUE) {
                ts = frame->pts;
            }

            double ptsSeconds = 0.0;
            if (ts != AV_NOPTS_VALUE && streamInfo.timeBase.den > 0) {
                ptsSeconds = ts * av_q2d(streamInfo.timeBase);
            } else {
                ptsSeconds = frameIndex / fallbackFps;
            }

            if (!haveStartPts) {
                haveStartPts = true;
                startPtsSeconds = ptsSeconds;
                wallClock.start();
            }

            const int frameWidth = frame->width > 0 ? frame->width : codecCtx->width;
            const int frameHeight = frame->height > 0 ? frame->height : codecCtx->height;
            const AVPixelFormat frameFormat = frame->format != AV_PIX_FMT_NONE
                                                  ? static_cast<AVPixelFormat>(frame->format)
                                                  : codecCtx->pix_fmt;

            if (frameWidth <= 0 || frameHeight <= 0 || frameFormat == AV_PIX_FMT_NONE) {
                emit finishedWithError("Decoded frame has invalid size or pixel format.");
                *fatalError = true;
                break;
            }

            if (!swsCtx
                || swsSourceWidth != frameWidth
                || swsSourceHeight != frameHeight
                || swsSourceFormat != frameFormat) {
                swsCtx = sws_getCachedContext(swsCtx,
                                              frameWidth,
                                              frameHeight,
                                              frameFormat,
                                              frameWidth,
                                              frameHeight,
                                              AV_PIX_FMT_BGRA,
                                              SWS_BILINEAR,
                                              nullptr,
                                              nullptr,
                                              nullptr);
                if (!swsCtx) {
                    emit finishedWithError("sws_getCachedContext failed.");
                    *fatalError = true;
                    break;
                }

                swsSourceWidth = frameWidth;
                swsSourceHeight = frameHeight;
                swsSourceFormat = frameFormat;
            }

            double speedCopy = 1.0;
            {
                QMutexLocker locker(&m_stateMutex);
                speedCopy = m_speed;
            }

            const double targetMsD = ((ptsSeconds - startPtsSeconds) * 1000.0) / speedCopy;
            const qint64 targetMs = static_cast<qint64>(targetMsD);
            const qint64 elapsedMs = wallClock.isValid() ? wallClock.elapsed() : 0;

            if (targetMs > elapsedMs) {
                QMutexLocker locker(&m_stateMutex);
                const qint64 waitMs = qMin<qint64>(targetMs - elapsedMs, 100);
                m_stateChanged.wait(&m_stateMutex, static_cast<unsigned long>(waitMs));
            }

            uint8_t *dstData[4] = { nullptr, nullptr, nullptr, nullptr };
            int dstLinesize[4] = { 0, 0, 0, 0 };
            const int dstBufferSize = av_image_alloc(dstData,
                                                     dstLinesize,
                                                     frameWidth,
                                                     frameHeight,
                                                     AV_PIX_FMT_BGRA,
                                                     1);
            if (dstBufferSize < 0) {
                emit finishedWithError(QString("av_image_alloc failed: %1").arg(ffmpegErrToString(dstBufferSize)));
                *fatalError = true;
                break;
            }

            sws_scale(swsCtx,
                      frame->data,
                      frame->linesize,
                      0,
                      frameHeight,
                      dstData,
                      dstLinesize);

            QImage convertedImage(dstData[0],
                                  frameWidth,
                                  frameHeight,
                                  dstLinesize[0],
                                  QImage::Format_ARGB32);
            emit frameReady(convertedImage.copy(), ptsSeconds);
            av_freep(&dstData[0]);

            emit playbackTimeChanged(ptsSeconds);
            frameIndex++;
            av_frame_unref(frame);
        }
    };

    bool fatalError = false;
    while (!shouldStop()) {
        waitWhilePaused();
        if (shouldStop()) {
            break;
        }

        if (!m_packetQueue->pop(packet)) {
            break;
        }

        err = avcodec_send_packet(codecCtx, packet);
        av_packet_unref(packet);
        if (err < 0) {
            emit finishedWithError(QString("avcodec_send_packet failed: %1").arg(ffmpegErrToString(err)));
            fatalError = true;
            break;
        }

        processAvailableFrames(&fatalError);
        if (fatalError) {
            break;
        }
    }

    if (!shouldStop() && !fatalError) {
        avcodec_send_packet(codecCtx, nullptr);
        processAvailableFrames(&fatalError);
    }

    if (swsCtx) {
        sws_freeContext(swsCtx);
    }
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&codecCtx);
}
