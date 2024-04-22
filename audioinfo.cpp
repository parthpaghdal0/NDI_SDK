
#include "audioinfo.h"

AudioInfo::AudioInfo(const QAudioDeviceInfo &deviceInfo)
{
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(1);
    format.setSampleSize(32);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::Float);

    if (!deviceInfo.isFormatSupported(format)) {
        format = deviceInfo.nearestFormat(format);
    }
    m_format = format;

    open(QIODevice::WriteOnly);

    m_audio = new QAudioInput(m_format, this);
}

AudioInfo::~AudioInfo() {
    m_audio->stop();
    close();

    delete m_audio;
}

void AudioInfo::start()
{
    m_audio->start(this);
}

void AudioInfo::stop()
{
    m_audio->stop();
}

qint64 AudioInfo::readData(char *data, qint64 maxlen)
{
    Q_UNUSED(data)
    Q_UNUSED(maxlen)

    return 0;
}

qint64 AudioInfo::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data)
    Q_UNUSED(len)

    emit emitAudioData((float*)data, len / sizeof(float));

    return len;
}
