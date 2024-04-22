#ifndef AUDIOINFO_H
#define AUDIOINFO_H

#include <QIODevice>
#include <QObject>
#include <QAudioFormat>
#include <QAudioInput>

class AudioInfo : public QIODevice
{
    Q_OBJECT

public:
    AudioInfo(const QAudioDeviceInfo &deviceInfo);
    ~AudioInfo();

    void start();
    void stop();

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

private:
    QAudioFormat m_format;
    QAudioInput* m_audio;

signals:
    void emitAudioData(float *data, qint64 len);
};

#endif // AUDIOINFO_H
