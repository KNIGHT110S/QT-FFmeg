#include "videodecodethread.h"

#include <QElapsedTimer>
#include <QImage>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
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

static double safeFps(const AVStream *stream)
{
    if (!stream) {
        return 0.0;
    }
    if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0) {
        return av_q2d(stream->avg_frame_rate);
    }
    if (stream->r_frame_rate.num > 0 && stream->r_frame_rate.den > 0) {
        return av_q2d(stream->r_frame_rate);
    }
    return 0.0;
}

} // namespace

VideoDecodeThread::VideoDecodeThread(QObject *parent)
    : QThread(parent)
{
}

VideoDecodeThread::~VideoDecodeThread()
{
    requestStop();
    wait(2000);
}

void VideoDecodeThread::setFilePath(const QString &filePath)
{
    QMutexLocker locker(&m_stateMutex);
    m_filePath = filePath;
}

void VideoDecodeThread::setSpeed(double speed)
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

void VideoDecodeThread::setPaused(bool paused)
{
    {
        QMutexLocker locker(&m_stateMutex);
        m_paused = paused;
    }
    m_stateChanged.wakeAll();
}

void VideoDecodeThread::requestStop()
{
    m_stopRequested.store(true, std::memory_order_relaxed);
    requestInterruption();
    m_stateChanged.wakeAll();
}

bool VideoDecodeThread::shouldStop() const
{
    return isInterruptionRequested() || m_stopRequested.load(std::memory_order_relaxed);
}

void VideoDecodeThread::waitWhilePaused()
{
    QMutexLocker locker(&m_stateMutex);
    while (!shouldStop() && m_paused) {
        m_stateChanged.wait(&m_stateMutex, 200);
    }
}

void VideoDecodeThread::run()
{
    QString filePathCopy;
    {
        QMutexLocker locker(&m_stateMutex);
        filePathCopy = m_filePath;
    }

    if (filePathCopy.isEmpty()) {
        emit finishedWithError("No file selected.");
        emit stopped();
        return;
    }

    AVFormatContext *formatCtx = nullptr;
    AVCodecContext *codecCtx = nullptr;
    SwsContext *swsCtx = nullptr;
    AVPacket *packet = nullptr;
    AVFrame *frame = nullptr;

    const QByteArray encoded = filePathCopy.toUtf8();
    int err = avformat_open_input(&formatCtx, encoded.constData(), nullptr, nullptr);
    if (err < 0) {
        emit finishedWithError(QString("avformat_open_input failed: %1").arg(ffmpegErrToString(err)));
        emit stopped();
        return;
    }

    err = avformat_find_stream_info(formatCtx, nullptr);
    if (err < 0) {
        emit finishedWithError(QString("avformat_find_stream_info failed: %1").arg(ffmpegErrToString(err)));
        avformat_close_input(&formatCtx);
        emit stopped();
        return;
    }

    const int videoStreamIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex < 0) {
        emit finishedWithError("No video stream found.");
        avformat_close_input(&formatCtx);
        emit stopped();
        return;
    }

    AVStream *videoStream = formatCtx->streams[videoStreamIndex];
    const AVCodecParameters *params = videoStream->codecpar;
    const AVCodec *decoder = avcodec_find_decoder(params->codec_id);
    if (!decoder) {
        emit finishedWithError("No suitable video decoder found.");
        avformat_close_input(&formatCtx);
        emit stopped();
        return;
    }

    codecCtx = avcodec_alloc_context3(decoder);
    if (!codecCtx) {
        emit finishedWithError("avcodec_alloc_context3 failed.");
        avformat_close_input(&formatCtx);
        emit stopped();
        return;
    }

    err = avcodec_parameters_to_context(codecCtx, params);
    if (err < 0) {
        emit finishedWithError(QString("avcodec_parameters_to_context failed: %1").arg(ffmpegErrToString(err)));
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        emit stopped();
        return;
    }

    err = avcodec_open2(codecCtx, decoder, nullptr);
    if (err < 0) {
        emit finishedWithError(QString("avcodec_open2 failed: %1").arg(ffmpegErrToString(err)));
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        emit stopped();
        return;
    }

    const double fps = safeFps(videoStream);
    double durationSeconds = 0.0;
    if (formatCtx->duration > 0) {
        durationSeconds = formatCtx->duration / static_cast<double>(AV_TIME_BASE);
    } else if (videoStream->duration > 0 && videoStream->time_base.den > 0) {
        durationSeconds = videoStream->duration * av_q2d(videoStream->time_base);
    }

    emit opened(durationSeconds, codecCtx->width, codecCtx->height, fps);

    packet = av_packet_alloc();
    frame = av_frame_alloc();
    if (!packet || !frame) {
        emit finishedWithError("Failed to allocate AVPacket/AVFrame.");
        av_packet_free(&packet);
        av_frame_free(&frame);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        emit stopped();
        return;
    }

    bool haveStartPts = false;
    double startPtsSeconds = 0.0;
    QElapsedTimer wallClock;
    wallClock.invalidate();

    qint64 frameIndex = 0;
    const double fallbackFps = (fps > 1e-6) ? fps : 25.0;
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
            if (ts != AV_NOPTS_VALUE && videoStream->time_base.den > 0) {
                ptsSeconds = ts * av_q2d(videoStream->time_base);
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

    while (!shouldStop()) {
        waitWhilePaused();
        if (shouldStop()) {
            break;
        }

        err = av_read_frame(formatCtx, packet);
        if (err < 0) {
            bool fatalError = false;
            avcodec_send_packet(codecCtx, nullptr);
            processAvailableFrames(&fatalError);
            break;
        }

        if (packet->stream_index != videoStreamIndex) {
            av_packet_unref(packet);
            continue;
        }

        err = avcodec_send_packet(codecCtx, packet);
        av_packet_unref(packet);
        if (err < 0) {
            emit finishedWithError(QString("avcodec_send_packet failed: %1").arg(ffmpegErrToString(err)));
            break;
        }

        bool fatalError = false;
        processAvailableFrames(&fatalError);
        if (fatalError) {
            break;
        }
    }

    if (swsCtx) {
        sws_freeContext(swsCtx);
        swsCtx = nullptr;
    }
    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&formatCtx);

    emit stopped();
}
