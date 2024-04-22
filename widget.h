#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <Processing.NDI.Lib.h>

#include <QScreen>
#include <QCameraInfo>
#include <QCameraImageCapture>
#include <QAudioDeviceInfo>
#include <QAudioInput>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class AudioInfo;

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

public slots:
    void on_pb_start_clicked();
    void on_pb_stop_clicked();
    void on_pb_close_clicked();

    void on_cb_screen_video_currentIndexChanged(int );
    void on_cb_screen_compression_currentIndexChanged(int );
    void on_cb_screen_frame_rate_currentIndexChanged(int );
    void on_cb_screen_audio_currentIndexChanged(int );
    void on_cb_camera_video_currentIndexChanged(int );
    void on_cb_camera_compression_currentIndexChanged(int );
    void on_cb_camera_frame_rate_currentIndexChanged(int );
    void on_cb_camera_audio_currentIndexChanged(int );

    void on_screen_timeout();
    void on_camera_timeout();
    void on_audio_camera(float* data, qint64 len);
    void on_audio_screen(float* data, qint64 len);
    void on_camera_image(int id, const QImage&);

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *event);

private:
    Ui::Widget *ui;

    NDIlib_send_instance_t pNDI_send_camera;
    NDIlib_send_instance_t pNDI_send_screen;
    NDIlib_video_frame_v2_t NDI_video_frame_camera;
    NDIlib_video_frame_v2_t NDI_video_frame_screen;
    NDIlib_audio_frame_v2_t NDI_audio_frame_camera;
    NDIlib_audio_frame_v2_t NDI_audio_frame_screen;

    QList<QScreen*> m_screens;
    QList<QCameraInfo> m_cameras;
    QList<QAudioDeviceInfo> m_audios;

    QTimer* m_screenTimer;
    QTimer* m_cameraTimer;

    QScreen* m_curScreen;
    QCamera* m_curCamera;
    AudioInfo* m_curAudioScreen;
    AudioInfo* m_curAudioCamera;
    QCameraImageCapture* m_imageCapture;

    void makeVideoFrame_RGBA(NDIlib_video_frame_v2_t *frame, QPixmap pix);
    void makeVideoFrame_UYVY(NDIlib_video_frame_v2_t *frame, QPixmap pix);
    void clearFrames();

    QPoint m_prevPos;
    bool m_pressed;
};
#endif // WIDGET_H
