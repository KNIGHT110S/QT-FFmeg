#include "packetqueue.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

PacketQueue::PacketQueue(int maxPackets)
    : m_maxPackets(maxPackets > 0 ? maxPackets : 96)
{
}

PacketQueue::~PacketQueue()
{
    abort();
    flush();
}

bool PacketQueue::push(const AVPacket *packet)
{
    if (!packet) {
        return false;
    }

    AVPacket *packetCopy = av_packet_clone(packet);
    if (!packetCopy) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    while (!m_aborted && !m_finished && static_cast<int>(m_packets.size()) >= m_maxPackets) {
        m_notFull.wait(&m_mutex, 100);
    }

    if (m_aborted || m_finished) {
        locker.unlock();
        av_packet_free(&packetCopy);
        return false;
    }

    m_packets.push_back(packetCopy);
    m_notEmpty.wakeOne();
    return true;
}

bool PacketQueue::pop(AVPacket *packet)
{
    if (!packet) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    while (!m_aborted && m_packets.empty() && !m_finished) {
        m_notEmpty.wait(&m_mutex, 100);
    }

    if (m_aborted) {
        return false;
    }

    if (m_packets.empty()) {
        return false;
    }

    AVPacket *storedPacket = m_packets.front();
    m_packets.pop_front();
    m_notFull.wakeOne();
    locker.unlock();

    av_packet_unref(packet);
    av_packet_move_ref(packet, storedPacket);
    av_packet_free(&storedPacket);
    return true;
}

void PacketQueue::setFinished()
{
    QMutexLocker locker(&m_mutex);
    m_finished = true;
    m_notEmpty.wakeAll();
    m_notFull.wakeAll();
}

void PacketQueue::abort()
{
    QMutexLocker locker(&m_mutex);
    m_aborted = true;
    m_notEmpty.wakeAll();
    m_notFull.wakeAll();
}

void PacketQueue::flush()
{
    QMutexLocker locker(&m_mutex);
    while (!m_packets.empty()) {
        AVPacket *packet = m_packets.front();
        m_packets.pop_front();
        av_packet_free(&packet);
    }
    m_notFull.wakeAll();
}

bool PacketQueue::isFinished() const
{
    QMutexLocker locker(&m_mutex);
    return m_finished;
}
