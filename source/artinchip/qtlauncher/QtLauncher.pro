#-------------------------------------------------
#
# Project created by QtCreator 2022-06-22T19:41:00
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qtlauncher
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

GE_SUPPORT = $$(QTLAUNCHER_GE_SUPPORT)
SMALL_MEMORY = $$(QTLAUNCHER_SMALL_MEMORY)
WIFI_MANAGER = $$(QTLAUNCHER_WIFI_MANAGER)

contains(GE_SUPPORT,YES){
DEFINES += QTLAUNCHER_GE_SUPPORT
LIBS += -L$$(STAGING_DIR)/usr/local/lib/ -lmpp_decoder -lmpp_ge -lmpp_ve -lmpp_base
}
contains(SMALL_MEMORY,YES){
DEFINES += QTLAUNCHER_SMALL_MEMORY
}
contains(WIFI_MANAGER,YES){
QMAKE_CXXFLAGS += -std=c++0x
CONFIG += c++11

DEFINES += QTLAUNCHER_WIFI_MANAGER
LIBS += -L$$(TARGET_DIR)/usr/local/lib/  -lwifimanager -lwpa_client -lrt
}

contains(GE_SUPPORT,YES) || contains(WIFI_MANAGER,YES){
target.path += /usr/local/launcher
INCLUDEPATH += $$(STAGING_DIR)/usr/include/
INCLUDEPATH += $$(TARGET_DIR)/usr/local/include/
INCLUDEPATH += $$(TARGET_DIR)/include/
INCLUDEPATH += $$(STAGING_DIR)/usr/local/include/
INSTALLS += target
}
# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

RESOURCES += qtlauncher.qrc

SOURCES += main.cpp\
        mainwindow.cpp \
        aicbasewindow.cpp \
    widgets/aicnavigationbar.cpp \
    widgets/aicstatusbar.cpp \
    widgets/aicdesktopbutton.cpp \
    widgets/aicdatetimebutton.cpp \
    widgets/aictimewidget.cpp \
    views/aicdatetimeview.cpp \
    views/aiccentralview.cpp \
    views/aicbriefview.cpp \
    views/aicanimaimageview.cpp \
    views/aicanimastateview.cpp \
    views/aicanimatilesview.cpp \
    widgets/aicgraphicsbutton.cpp \
    views/aicimageview.cpp \
    views/aicscaleview.cpp \
    views/aicrtpview.cpp \
    views/aicvideoview.cpp \
    video/aicdecodethread.cpp \
    video/aicrenderthread.cpp \
    video/aicvideothread.cpp \
    video/aicimagedecoder.cpp \
    views/aicdashboardview.cpp

contains(WIFI_MANAGER,YES){
SOURCES +=     views/aicconfigview.cpp \
    keyboard/Keyboard.cpp \
    keyboard/KeyButton.cpp \
    wifi/aicwifithread.cpp
}

HEADERS  += mainwindow.h \
    aicbasewindow.h \
    widgets/aicnavigationbar.h \
    widgets/aicstatusbar.h \
    widgets/aicdesktopbutton.h \
    utils/aictypes.h \
    widgets/aicdatetimebutton.h \
    widgets/aictimewidget.h \
    views/aicdatetimeview.h \
    views/aiccentralview.h \
    views/aicbriefview.h \
    views/aicanimaimageview.h \
    views/aicanimastateview.h \
    views/aicanimatilesview.h \
    widgets/aicgraphicsbutton.h \
    views/aicimageview.h \
    views/aicscaleview.h \
    utils/aicconsts.h \
    views/aicrtpview.h \
    views/aicvideoview.h \
    video/aicdecodethread.h \
    video/aicrenderthread.h \
    video/aicvideothread.h \
    video/aicimagedecoder.h \
    views/aicdashboardview.h

contains(WIFI_MANAGER,YES){
HEADERS  += views/aicconfigview.h \
    keyboard/Keyboard.h \
    keyboard/KeyButton.h \
    keyboard/AbstractKeyboard.h \
    wifi/aicwifithread.h
}

DISTFILES += \
    resources/brief.png
