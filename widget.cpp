#include "widget.h"
#include "ui_widget.h"

#include <QDebug>

#include "utils.h"

float frameRates[] = { 60, 60, 50, 30, 30, 25 };

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    m_screens.push_back(0);
    QList<QScreen*> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen *screen = screens.at(i);
        ui->cb_screen_video->addItem(screen->name());
        m_screens.push_back(screen);
    }

    m_cameras.push_back(QCameraInfo());
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    for (const QCameraInfo &cameraInfo : cameras) {
        QString name = cameraInfo.description();
        if (name.contains("NDI Webcam")) continue;
        ui->cb_camera_video->addItem(name);
        m_cameras.push_back(cameraInfo);
    }

    m_audios.push_back(QAudioDeviceInfo());
    QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    for (const QAudioDeviceInfo &device : devices) {
        QString name = device.deviceName();
        if (name.contains("NDI Webcam")) continue;
        ui->cb_camera_audio->addItem(name);
        ui->cb_screen_audio->addItem(name);
        m_audios.push_back(device);
    }

    NDIlib_send_create_t NDI_send_create_desc_camera;
    NDI_send_create_desc_camera.p_ndi_name = "My Camera";
    pNDI_send_camera = NDIlib_send_create(&NDI_send_create_desc_camera);

    NDIlib_send_create_t NDI_send_create_desc_screen;
    NDI_send_create_desc_screen.p_ndi_name = "My Screen";
    pNDI_send_screen = NDIlib_send_create(&NDI_send_create_desc_screen);

    m_curCamera = NULL;
    m_curScreen = NULL;
    m_imageCapture = NULL;

    NDI_video_frame_camera.p_data = NULL;
    NDI_video_frame_screen.p_data = NULL;

    m_screenTimer = new QTimer(this);
    connect(m_screenTimer, SIGNAL(timeout()), this, SLOT(on_screen_timeout()));
    m_cameraTimer = new QTimer(this);
    connect(m_cameraTimer, SIGNAL(timeout()), this, SLOT(on_camera_timeout()));

    ui->pb_start->setEnabled(true);
    ui->pb_stop->setEnabled(false);
}

Widget::~Widget()
{
    on_pb_stop_clicked();

    if (pNDI_send_camera)
        NDIlib_send_destroy(pNDI_send_camera);
    if (pNDI_send_screen)
        NDIlib_send_destroy(pNDI_send_screen);

    delete ui;
}

void Widget::clearFrames()
{
    if (NDI_video_frame_camera.p_data)
        free(NDI_video_frame_camera.p_data);
    NDI_video_frame_camera.p_data = NULL;

    if (NDI_video_frame_screen.p_data)
        free(NDI_video_frame_screen.p_data);
    NDI_video_frame_screen.p_data = NULL;
}

void Widget::on_pb_start_clicked()
{
    ui->pb_start->setEnabled(false);
    ui->pb_stop->setEnabled(true);

    int screenIndex = ui->cb_screen_video->currentIndex();
    int cameraIndex = ui->cb_camera_video->currentIndex();
    int screenAudioIndex = ui->cb_screen_audio->currentIndex();
    int cameraAudioIndex = ui->cb_camera_audio->currentIndex();
    int screenComp = ui->cb_screen_compression->currentIndex();
    int cameraComp = ui->cb_camera_compression->currentIndex();

    m_curScreen = m_screens[screenIndex];
    NDI_video_frame_screen.xres = m_curScreen ? m_curScreen->size().width() : 0;
    NDI_video_frame_screen.yres = m_curScreen ? m_curScreen->size().height() : 0;
    NDI_video_frame_screen.FourCC = screenComp == 0 ? NDIlib_FourCC_video_type_UYVY : NDIlib_FourCC_video_type_RGBA;
    NDI_video_frame_screen.p_data = (uint8_t*)malloc(NDI_video_frame_screen.xres * NDI_video_frame_screen.yres * 4 / (2 - screenComp));

    Q_ASSERT(m_curCamera == NULL && m_imageCapture == NULL);
    m_curCamera = new QCamera(m_cameras[cameraIndex]);
    m_imageCapture = new QCameraImageCapture(m_curCamera);
    m_imageCapture->setCaptureDestination(QCameraImageCapture::CaptureToBuffer);
    QObject::connect(m_imageCapture, SIGNAL(imageCaptured(int, const QImage&)), this, SLOT(on_camera_image(int, const QImage&)));
    m_curCamera->start();
    QSize camSize = m_curCamera->supportedViewfinderResolutions().last();
    NDI_video_frame_camera.xres = camSize.width();
    NDI_video_frame_camera.yres = camSize.height();
    NDI_video_frame_camera.FourCC = cameraComp == 0 ? NDIlib_FourCC_video_type_UYVY : NDIlib_FourCC_video_type_RGBA;
    NDI_video_frame_camera.p_data = (uint8_t*)malloc(NDI_video_frame_camera.xres * NDI_video_frame_camera.yres * 4 / (2 - cameraComp));

    QAudioDeviceInfo screenAudio = m_audios[screenAudioIndex];
    QAudioDeviceInfo cameraAudio = m_audios[cameraAudioIndex];

    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    if (!screenAudio.isFormatSupported(format)) {
        format = screenAudio.nearestFormat(format);
    }

    m_screenTimer->start(1000.0f / frameRates[ui->cb_screen_frame_rate->currentIndex()]);
    m_cameraTimer->start(1000.0f / frameRates[ui->cb_camera_frame_rate->currentIndex()]);
}

void Widget::on_pb_stop_clicked()
{
    ui->pb_start->setEnabled(true);
    ui->pb_stop->setEnabled(false);

    clearFrames();
    m_screenTimer->stop();
    m_cameraTimer->stop();

    if (m_curCamera) {
        m_curCamera->stop();
        m_curCamera->unload();
    }
    delete m_curCamera;
    m_curCamera = NULL;

    if (m_imageCapture) {
        m_imageCapture->cancelCapture();
        delete m_imageCapture;
    }
    m_imageCapture = NULL;
}

void Widget::makeVideoFrame_RGBA(NDIlib_video_frame_v2_t *frame, QPixmap pix)
{
    QImage img = pix.toImage();
    if (img.format() != QImage::Format_RGBA8888)
        img = img.convertToFormat(QImage::Format_RGBA8888);
    Q_ASSERT(img.byteCount() == pix.width() * pix.height() * 4);

    const uchar* data = img.constBits();
    memcpy((void*)frame->p_data, data, img.byteCount());
}

void Widget::makeVideoFrame_UYVY(NDIlib_video_frame_v2_t *frame, QPixmap pix)
{
    std::vector<unsigned char> data = convertQPixmapToUYVY(pix);
    Q_ASSERT(data.size() == pix.width() * pix.height() * 2);

    memcpy((void*)frame->p_data, data.data(), data.size());
}

void Widget::on_screen_timeout()
{
    if (m_curScreen) {
        QPixmap screen = grabWindow(0, m_curScreen->geometry());
        if (ui->cb_screen_compression->currentIndex() == 0)
            makeVideoFrame_UYVY(&NDI_video_frame_screen, screen);
        else
            makeVideoFrame_RGBA(&NDI_video_frame_screen, screen);
        NDIlib_send_send_video_v2(pNDI_send_screen, &NDI_video_frame_screen);
    }
}

void Widget::on_camera_timeout()
{
    if (m_imageCapture && m_imageCapture->isReadyForCapture()) {
        m_imageCapture->capture();
    }
}

void Widget::on_camera_image(int id, const QImage& img)
{
    if (ui->cb_camera_compression->currentIndex() == 0)
        makeVideoFrame_UYVY(&NDI_video_frame_camera, QPixmap::fromImage(img));
    else
        makeVideoFrame_RGBA(&NDI_video_frame_camera, QPixmap::fromImage(img));
    NDIlib_send_send_video_v2(pNDI_send_camera, &NDI_video_frame_camera);
}

void Widget::on_cb_camera_audio_currentIndexChanged(int index)
{
    if (ui->pb_start->isEnabled()) return;
    on_pb_stop_clicked();
    on_pb_start_clicked();
}

void Widget::on_cb_camera_compression_currentIndexChanged(int index)
{
    if (ui->pb_start->isEnabled()) return;
    on_pb_stop_clicked();
    on_pb_start_clicked();
}

void Widget::on_cb_camera_frame_rate_currentIndexChanged(int index)
{
    if (ui->pb_start->isEnabled()) return;
    on_pb_stop_clicked();
    on_pb_start_clicked();
}

void Widget::on_cb_camera_video_currentIndexChanged(int index)
{
    if (ui->pb_start->isEnabled()) return;
    on_pb_stop_clicked();
    on_pb_start_clicked();
}

void Widget::on_cb_screen_audio_currentIndexChanged(int index)
{
    if (ui->pb_start->isEnabled()) return;
    on_pb_stop_clicked();
    on_pb_start_clicked();
}

void Widget::on_cb_screen_compression_currentIndexChanged(int index)
{
    if (ui->pb_start->isEnabled()) return;
    on_pb_stop_clicked();
    on_pb_start_clicked();
}

void Widget::on_cb_screen_frame_rate_currentIndexChanged(int index)
{
    if (ui->pb_start->isEnabled()) return;
    on_pb_stop_clicked();
    on_pb_start_clicked();
}

void Widget::on_cb_screen_video_currentIndexChanged(int index)
{
    if (ui->pb_start->isEnabled()) return;
    on_pb_stop_clicked();
    on_pb_start_clicked();
}
