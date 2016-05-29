#-------------------------------------------------
#
# Project created by QtCreator 2016-04-19T21:27:31
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = TestProto
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    proto.cpp

HEADERS  += mainwindow.h \
    proto.h

FORMS    += mainwindow.ui
