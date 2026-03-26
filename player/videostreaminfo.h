#ifndef VIDEOSTREAMINFO_H
#define VIDEOSTREAMINFO_H

#include <QMetaType>

extern "C" {
#include <libavcodec/avcodec.h>
}

struct VideoStreamInfo
{
    VideoStreamInfo();
    VideoStreamInfo(const VideoStreamInfo &other);
    VideoStreamInfo(VideoStreamInfo &&other) noexcept;
    ~VideoStreamInfo();

    VideoStreamInfo &operator=(const VideoStreamInfo &other);
    VideoStreamInfo &operator=(VideoStreamInfo &&other) noexcept;

    void clear();
    bool copyFrom(const AVCodecParameters *sourceParameters,
                  AVRational sourceTimeBase,
                  double sourceDurationSeconds,
                  double sourceFps);
    bool isValid() const;

    const AVCodecParameters *codecParameters() const;

    AVRational timeBase{0, 1};
    double durationSeconds = 0.0;
    double fps = 0.0;
    int width = 0;
    int height = 0;

private:
    AVCodecParameters *m_codecParameters = nullptr;
};

Q_DECLARE_METATYPE(VideoStreamInfo)

#endif // VIDEOSTREAMINFO_H
