#-------------------------------------------------
#
# Project created by QtCreator 2014-05-21T16:46:47
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = confuse
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += \
    fwup.c \
    sha2.c

LIBS += -lconfuse -larchive

HEADERS += \
    sha2.h
