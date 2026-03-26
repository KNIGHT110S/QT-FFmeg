#include "videostreaminfo.h"

#include <utility>

namespace {

static AVCodecParameters *cloneCodecParameters(const AVCodecParameters *source)
{
    if (!source) {
        return nullptr;
    }

    AVCodecParameters *copy = avcodec_parameters_alloc();
    if (!copy) {
        return nullptr;
    }

    if (avcodec_parameters_copy(copy, source) < 0) {
        avcodec_parameters_free(&copy);
        return nullptr;
    }

    return copy;
}

} // namespace

VideoStreamInfo::VideoStreamInfo() = default;

VideoStreamInfo::VideoStreamInfo(const VideoStreamInfo &other)
{
    *this = other;
}

VideoStreamInfo::VideoStreamInfo(VideoStreamInfo &&other) noexcept
    : timeBase(other.timeBase)
    , durationSeconds(other.durationSeconds)
    , fps(other.fps)
    , width(other.width)
    , height(other.height)
    , m_codecParameters(other.m_codecParameters)
{
    other.timeBase = AVRational{0, 1};
    other.durationSeconds = 0.0;
    other.fps = 0.0;
    other.width = 0;
    other.height = 0;
    other.m_codecParameters = nullptr;
}

VideoStreamInfo::~VideoStreamInfo()
{
    clear();
}

VideoStreamInfo &VideoStreamInfo::operator=(const VideoStreamInfo &other)
{
    if (this == &other) {
        return *this;
    }

    AVCodecParameters *copy = cloneCodecParameters(other.m_codecParameters);
    clear();

    m_codecParameters = copy;
    timeBase = other.timeBase;
    durationSeconds = other.durationSeconds;
    fps = other.fps;
    width = other.width;
    height = other.height;
    return *this;
}

VideoStreamInfo &VideoStreamInfo::operator=(VideoStreamInfo &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    clear();

    timeBase = other.timeBase;
    durationSeconds = other.durationSeconds;
    fps = other.fps;
    width = other.width;
    height = other.height;
    m_codecParameters = other.m_codecParameters;

    other.timeBase = AVRational{0, 1};
    other.durationSeconds = 0.0;
    other.fps = 0.0;
    other.width = 0;
    other.height = 0;
    other.m_codecParameters = nullptr;
    return *this;
}

void VideoStreamInfo::clear()
{
    if (m_codecParameters) {
        avcodec_parameters_free(&m_codecParameters);
    }
    timeBase = AVRational{0, 1};
    durationSeconds = 0.0;
    fps = 0.0;
    width = 0;
    height = 0;
}

bool VideoStreamInfo::copyFrom(const AVCodecParameters *sourceParameters,
                               AVRational sourceTimeBase,
                               double sourceDurationSeconds,
                               double sourceFps)
{
    AVCodecParameters *copy = cloneCodecParameters(sourceParameters);
    if (!copy) {
        clear();
        return false;
    }

    clear();
    m_codecParameters = copy;
    timeBase = sourceTimeBase;
    durationSeconds = sourceDurationSeconds;
    fps = sourceFps;
    width = copy->width;
    height = copy->height;
    return true;
}

bool VideoStreamInfo::isValid() const
{
    return m_codecParameters != nullptr && m_codecParameters->codec_id != AV_CODEC_ID_NONE;
}

const AVCodecParameters *VideoStreamInfo::codecParameters() const
{
    return m_codecParameters;
}
