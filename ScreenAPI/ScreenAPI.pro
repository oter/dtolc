#-------------------------------------------------
#
# Project created by QtCreator 2016-06-02T21:09:34
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ScreenAPI
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    crc16.c

HEADERS  += mainwindow.h \
    screen_proto.h \
    crc16.h

FORMS    += mainwindow.ui
