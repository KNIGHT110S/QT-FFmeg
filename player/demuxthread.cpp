#include "demuxthread.h"

#include "packetqueue.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
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

DemuxThread::DemuxThread(QObject *parent)
    : QThread(parent)
{
}

DemuxThread::~DemuxThread()
{
    requestStop();
    wait(2000);
}

void DemuxThread::setFilePath(const QString &filePath)
{
    m_filePath = filePath;
}

void DemuxThread::setPacketQueue(PacketQueue *packetQueue)
{
    m_packetQueue = packetQueue;
}

void DemuxThread::requestStop()
{
    m_stopRequested.store(true, std::memory_order_relaxed);
    requestInterruption();
    if (m_packetQueue) {
        m_packetQueue->abort();
    }
}

bool DemuxThread::shouldStop() const
{
    return isInterruptionRequested() || m_stopRequested.load(std::memory_order_relaxed);
}

void DemuxThread::run()
{
    if (m_filePath.isEmpty()) {
        emit finishedWithError("No file selected.");
        return;
    }

    if (!m_packetQueue) {
        emit finishedWithError("Packet queue was not initialized.");
        return;
    }

    AVFormatContext *formatCtx = nullptr;
    AVPacket *packet = nullptr;

    const QByteArray encodedFilePath = m_filePath.toUtf8();
    int err = avformat_open_input(&formatCtx, encodedFilePath.constData(), nullptr, nullptr);
    if (err < 0) {
        emit finishedWithError(QString("avformat_open_input failed: %1").arg(ffmpegErrToString(err)));
        return;
    }

    err = avformat_find_stream_info(formatCtx, nullptr);
    if (err < 0) {
        emit finishedWithError(QString("avformat_find_stream_info failed: %1").arg(ffmpegErrToString(err)));
        avformat_close_input(&formatCtx);
        return;
    }

    const int videoStreamIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex < 0) {
        emit finishedWithError("No video stream found.");
        avformat_close_input(&formatCtx);
        return;
    }

    AVStream *videoStream = formatCtx->streams[videoStreamIndex];
    double durationSeconds = 0.0;
    if (formatCtx->duration > 0) {
        durationSeconds = formatCtx->duration / static_cast<double>(AV_TIME_BASE);
    } else if (videoStream->duration > 0 && videoStream->time_base.den > 0) {
        durationSeconds = videoStream->duration * av_q2d(videoStream->time_base);
    }

    VideoStreamInfo streamInfo;
    if (!streamInfo.copyFrom(videoStream->codecpar,
                             videoStream->time_base,
                             durationSeconds,
                             safeFps(videoStream))) {
        emit finishedWithError("Failed to copy video stream parameters.");
        avformat_close_input(&formatCtx);
        return;
    }

    emit streamInfoReady(streamInfo);

    packet = av_packet_alloc();
    if (!packet) {
        emit finishedWithError("Failed to allocate AVPacket.");
        avformat_close_input(&formatCtx);
        return;
    }

    while (!shouldStop()) {
        err = av_read_frame(formatCtx, packet);
        if (err == AVERROR_EOF) {
            m_packetQueue->setFinished();
            break;
        }
        if (err < 0) {
            emit finishedWithError(QString("av_read_frame failed: %1").arg(ffmpegErrToString(err)));
            m_packetQueue->abort();
            break;
        }

        if (packet->stream_index == videoStreamIndex && !m_packetQueue->push(packet)) {
            av_packet_unref(packet);
            break;
        }

        av_packet_unref(packet);
    }

    if (shouldStop()) {
        m_packetQueue->abort();
    }

    av_packet_free(&packet);
    avformat_close_input(&formatCtx);
}
