#include "widget.h"
#include "ui_widget.h"

#include <QDebug>
#include <QMouseEvent>
#include <QPainter>

#include "utils.h"
#include "global.h"
#include "audioinfo.h"

float frameRates[] = { 60, 60, 50, 30, 30, 25 };

SenderThread::SenderThread(NDIlib_send_instance_t instance, NDIlib_video_frame_v2_t* video_frame, NDIlib_audio_frame_v2_t* audio_frame, QObject *parent) : QThread(parent){
    m_instance = instance;
    m_video_frame = video_frame;
    m_audio_frame = audio_frame;
}

void SenderThread::Start() {
    m_isRunning = true;
    start();
}

void SenderThread::Stop() {
    m_isRunning = false;
    wait();
    quit();
}

void SenderThread::Push(SendData *data) {
    m_mutex.lock();
    m_data.push_back(data);
    m_mutex.unlock();
}

void SenderThread::run() {
    while (m_isRunning) {
        m_mutex.lock();
        if (m_data.empty()) {
            m_mutex.unlock();
            continue;
        }
        SendData* data = m_data.first();
        m_mutex.unlock();
        m_data.pop_front();

        if (m_video_frame) {
            memmove(m_video_frame->p_data, data->buf, data->len);
            NDIlib_send_send_video_v2(m_instance, m_video_frame);
        }
        if (m_audio_frame) {
            memmove(m_audio_frame->p_data, data->buf, data->len);
            NDIlib_send_send_audio_v2(m_instance, m_audio_frame);
        }
        delete data->buf;

        delete data;
    }
}

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    setStyleSheet(loadScss("main"));

    setMouseTracking(false);
    m_pressed = false;

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
    m_curAudioCamera = NULL;
    m_curAudioScreen = NULL;
    m_imageCapture = NULL;

    m_senderCamera = new SenderThread(pNDI_send_camera, &NDI_video_frame_camera, NULL);
    m_senderScreen = new SenderThread(pNDI_send_screen, &NDI_video_frame_screen, NULL);
    m_senderAudioCamera = new SenderThread(pNDI_send_camera, NULL, &NDI_audio_frame_camera);
    m_senderAudioScreen = new SenderThread(pNDI_send_screen, NULL, &NDI_audio_frame_screen);

    NDI_video_frame_camera.p_data = NULL;
    NDI_video_frame_screen.p_data = NULL;
    NDI_audio_frame_camera.p_data = NULL;
    NDI_audio_frame_screen.p_data = NULL;

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
    QSize camSize = m_curCamera->viewfinderSettings().resolution();
    NDI_video_frame_camera.xres = camSize.width();
    NDI_video_frame_camera.yres = camSize.height();
    NDI_video_frame_camera.FourCC = cameraComp == 0 ? NDIlib_FourCC_video_type_UYVY : NDIlib_FourCC_video_type_RGBA;
    NDI_video_frame_camera.p_data = (uint8_t*)malloc(NDI_video_frame_camera.xres * NDI_video_frame_camera.yres * 4 / (2 - cameraComp));

    QAudioDeviceInfo screenAudio = m_audios[screenAudioIndex];
    QAudioDeviceInfo cameraAudio = m_audios[cameraAudioIndex];

    NDI_audio_frame_screen.sample_rate = 44100;
    NDI_audio_frame_screen.no_channels = 1;
    NDI_audio_frame_screen.no_samples = 1764;
    NDI_audio_frame_screen.p_data = (float*)malloc(sizeof(float) * 1764);

    NDI_audio_frame_camera.sample_rate = 44100;
    NDI_audio_frame_camera.no_channels = 1;
    NDI_audio_frame_camera.no_samples = 1764;
    NDI_audio_frame_camera.p_data = (float*)malloc(sizeof(float) * 1764);

    m_curAudioScreen = new AudioInfo(screenAudio);
    m_curAudioCamera = new AudioInfo(cameraAudio);
    connect(m_curAudioCamera, SIGNAL(emitAudioData(float*, qint64)), this, SLOT(on_audio_camera(float*, qint64)));
    connect(m_curAudioScreen, SIGNAL(emitAudioData(float*, qint64)), this, SLOT(on_audio_screen(float*, qint64)));

    m_curAudioScreen->start();
    m_curAudioCamera->start();

    m_screenTimer->start(1000.0f / frameRates[ui->cb_screen_frame_rate->currentIndex()]);
    m_cameraTimer->start(1000.0f / frameRates[ui->cb_camera_frame_rate->currentIndex()]);

    m_senderCamera->Start();
    m_senderScreen->Start();
    m_senderAudioCamera->Start();
    m_senderAudioScreen->Start();
}

void Widget::on_audio_camera(float *data, qint64 len)
{
    SendData* sdata = new SendData;
    sdata->buf = new char[len * sizeof(float)];
    sdata->len = len * sizeof(float);
    memcpy((void*)sdata->buf, data, len * sizeof(float));

    m_senderAudioCamera->Push(sdata);
}

void Widget::on_audio_screen(float *data, qint64 len)
{
    SendData* sdata = new SendData;
    sdata->buf = new char[len * sizeof(float)];
    sdata->len = len * sizeof(float);
    memcpy((void*)sdata->buf, data, len * sizeof(float));

    m_senderAudioScreen->Push(sdata);
}

void Widget::on_pb_stop_clicked()
{
    m_senderCamera->Stop();
    m_senderScreen->Stop();
    m_senderAudioCamera->Stop();
    m_senderAudioScreen->Stop();

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

    if (m_curAudioCamera)
        delete m_curAudioCamera;
    m_curAudioCamera = NULL;

    if (m_curAudioScreen)
        delete m_curAudioScreen;
    m_curAudioScreen = NULL;

    free(NDI_video_frame_camera.p_data);
    free(NDI_video_frame_screen.p_data);
    free(NDI_audio_frame_camera.p_data);
    free(NDI_audio_frame_screen.p_data);
}

void Widget::on_pb_close_clicked()
{
    exit(0);
}

void Widget::makeVideoFrame_RGBA(SendData *sdata, QPixmap pix)
{
    QImage img = pix.toImage();
    if (img.format() != QImage::Format_RGBA8888)
        img = img.convertToFormat(QImage::Format_RGBA8888);
    Q_ASSERT(img.byteCount() == pix.width() * pix.height() * 4);

    const uchar* data = img.constBits();
    sdata->len = img.byteCount();
    sdata->buf = new char[sdata->len];
    memcpy((void*)sdata->buf, data, img.byteCount());
}

void Widget::makeVideoFrame_UYVY(SendData *sdata, QPixmap pix)
{
    std::vector<unsigned char> data = convertQPixmapToUYVY(pix);
    Q_ASSERT(data.size() == pix.width() * pix.height() * 2);

    sdata->len = data.size();
    sdata->buf = new char[sdata->len];
    memcpy((void*)sdata->buf, data.data(), data.size());
}

void Widget::on_screen_timeout()
{
    if (m_curScreen) {
        QPixmap screen = grabWindow(0, m_curScreen->geometry());
        SendData *data = new SendData;
        if (ui->cb_screen_compression->currentIndex() == 0)
            makeVideoFrame_UYVY(data, screen);
        else
            makeVideoFrame_RGBA(data, screen);
        m_senderScreen->Push(data);
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
    SendData *data = new SendData;
    if (ui->cb_camera_compression->currentIndex() == 0)
        makeVideoFrame_UYVY(data, QPixmap::fromImage(img));
    else
        makeVideoFrame_RGBA(data, QPixmap::fromImage(img));
    NDIlib_send_send_video_v2(pNDI_send_camera, &NDI_video_frame_camera);
    m_senderCamera->Push(data);
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

void Widget::mousePressEvent(QMouseEvent *event)
{
    for (const QObject *child : children()) {
        if (const QPushButton *button = qobject_cast<const QPushButton *>(child)) {
            if (button->geometry().contains(event->pos())) {
                return;
            }
        } else if (const QComboBox *comboBox = qobject_cast<const QComboBox *>(child)) {
            if (comboBox->geometry().contains(event->pos())) {
                return;
            }
        }
    }

    m_pressed = true;
    m_prevPos = event->globalPos();
}

void Widget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_pressed)
        move(event->globalPos() - m_prevPos + pos());
    m_prevPos = event->globalPos();
}

void Widget::mouseReleaseEvent(QMouseEvent *event)
{
    m_pressed = false;
}

void Widget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    QPen pen(QColor(25, 156, 244));
    pen.setWidth(2);
    painter.setPen(pen);

    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect(), 13, 13);
}
