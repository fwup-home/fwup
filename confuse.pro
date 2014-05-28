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
    sha2.c \
    fatfs.c \
    fatfs/src/ff.c \
    mbr.c \
    cfgfile.c \
    util.c \
    mmc.c \
    fwup_create.c \
    functions.c \
    fwup_apply.c \
    fwup_list.c

LIBS += -lconfuse -larchive

HEADERS += \
    sha2.h \
    fatfs.h \
    fatfs/src/ff.h \
    fatfs/src/ffconf.h \
    mbr.h \
    cfgfile.h \
    util.h \
    mmc.h \
    fwup_create.h \
    functions.h \
    fwup_apply.h \
    fwup_list.h

OTHER_FILES += \
    fwupdate.conf \
    README.md
