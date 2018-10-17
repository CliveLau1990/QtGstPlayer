#-------------------------------------------------
#
# Project created by QtCreator 2017-11-30T10:17:44
#
#-------------------------------------------------

QT       += core gui multimediawidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QtGsPlayer
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        widget.cpp \
    videowidget.cpp \
    playercontrols.cpp

HEADERS += \
        widget.h \
    videowidget.h \
    playercontrols.h

INCLUDEPATH += \
    /home/clivelau/Programs/fsl-imx-x11/4.9.11-1.0.0/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include/gstreamer-1.0 \
    /home/clivelau/Programs/fsl-imx-x11/4.9.11-1.0.0/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include/glib-2.0 \
    /home/clivelau/Programs/fsl-imx-x11/4.9.11-1.0.0/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include/libxml2 \
    /home/clivelau/Programs/fsl-imx-x11/4.9.11-1.0.0/sysroots/cortexa9hf-neon-poky-linux-gnueabi/usr/include

LIBS += -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lgstvideo-1.0

RESOURCES += \
    image.qrc

