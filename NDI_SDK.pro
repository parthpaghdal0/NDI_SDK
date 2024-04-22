QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets multimedia

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    audioinfo.cpp \
    global.cpp \
    main.cpp \
    widget.cpp

HEADERS += \
    audioinfo.h \
    global.h \
    utils.h \
    widget.h

FORMS += \
    widget.ui

INCLUDEPATH += "C:\Program Files\NDI\NDI 6 SDK\Include"
LIBS += -L"C:\Program Files\NDI\NDI 6 SDK\Lib\x64" -lProcessing.NDI.Lib.x64 -lgdi32

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    NDI_SDK.qrc
