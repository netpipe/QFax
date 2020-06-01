#-------------------------------------------------
#
# Project created by QtCreator 2019-05-26T05:05:06
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QFax
TEMPLATE = app

#QMAKE_CXXFLAGS += -fpermissive #-std=c++11
#CONFIG += c++11
#CONFIG += C++11

#INCLUDEPATH +=
#LIBS += -lgtk-3 -lgdk-3 -lpangocairo-1.0 -lpango-1.0 -latk-1.0 -lcairo-gobject -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0
#LIBS += -lgthread-2.0 -pthread


LIBS += -lm -L/usr/X11R6/lib -lX11
# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        mainwindow.cpp \
    linmodem/atparser.c \
    linmodem/dsp.c \
    linmodem/fsk.c \
    linmodem/lmreal.c \
    linmodem/lmsim.c \
    linmodem/lmsoundcard.c \
    linmodem/nodisplay.c \
    linmodem/serial.c \
    linmodem/v8.c \
    linmodem/v21.c \
    linmodem/v22.c \
    linmodem/v23.c \
    linmodem/v34.c \
    linmodem/v34eq.c \
    linmodem/v90.c \
    linmodem/v34table.c \
    linmodem/v90table.c \
    linmodem/dtmf.c \
    linmodem/lm.c \
    efax-gtk-3.2.13/efax/efax.c \
    efax-gtk-3.2.13/efax/efaxio.c \
    efax-gtk-3.2.13/efax/efaxlib.c \
    efax-gtk-3.2.13/efax/efaxmsg.c \
    efax-gtk-3.2.13/efax/efaxos.c \
    efax-gtk-3.2.13/efax-gtk-faxfilter/efax-gtk-socket-client.cpp

HEADERS += \
        mainwindow.h \
    linmodem/dsp.h \
    linmodem/dtmf.h \
    linmodem/fsk.h \
    linmodem/lmstates.h \
    linmodem/v8.h \
    linmodem/v21.h \
    linmodem/v22.h \
    linmodem/v23.h \
    linmodem/v34.h \
    linmodem/v90.h \
    linmodem/v90priv.h \
    linmodem/lm.h \
    linmodem/v34priv.h \
    linmodem/display.h \
    efax-gtk-3.2.13/efax/efaxio.h \
    efax-gtk-3.2.13/efax/efaxlib.h \
    efax-gtk-3.2.13/efax/efaxmsg.h \
    efax-gtk-3.2.13/efax/efaxos.h \
    efax-gtk-3.2.13/config.h

FORMS += \
        mainwindow.ui
