#ifndef VIDEORENDERWIDGET_H
#define VIDEORENDERWIDGET_H

#include <QImage>
#include <QMutex>
#include <QWidget>

class VideoRenderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoRenderWidget(QWidget *parent = nullptr);

public slots:
    void setFrame(const QImage &image);
    void clearFrame();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QMutex m_mutex;
    QImage m_frame;
};

#endif // VIDEORENDERWIDGET_H

