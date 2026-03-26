#include "videorenderwidget.h"

#include <QPainter>

VideoRenderWidget::VideoRenderWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
}

void VideoRenderWidget::setFrame(const QImage &image)
{
    {
        QMutexLocker locker(&m_mutex);
        m_frame = image;
    }
    update();
}

void VideoRenderWidget::clearFrame()
{
    {
        QMutexLocker locker(&m_mutex);
        m_frame = QImage();
    }
    update();
}

void VideoRenderWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(10, 14, 19));

    QImage frameCopy;
    {
        QMutexLocker locker(&m_mutex);
        frameCopy = m_frame;
    }

    if (frameCopy.isNull()) {
        painter.setPen(QColor(151, 164, 177));
        painter.drawText(rect(), Qt::AlignCenter, "Open a video to start");
        return;
    }

    const QSize targetSize = frameCopy.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect targetRect(QPoint((width() - targetSize.width()) / 2, (height() - targetSize.height()) / 2), targetSize);
    painter.drawImage(targetRect, frameCopy);
}
