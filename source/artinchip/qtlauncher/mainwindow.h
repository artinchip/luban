/*
 * Copyright (C) 2024-2025 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 *         Huahui Mai <huahui.mai@artinchip.com>
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QPoint>
#include <QTimer>

#include "aicbasewindow.h"
#include "widgets/aicnavigationbar.h"
#include "widgets/aicdatetimebutton.h"
#include "widgets/aicdesktopbutton.h"
#include "views/aiccentralview.h"
#include "views/aicbriefview.h"
#include "views/aicdatetimeview.h"
#include "views/aicanimaimageview.h"
#include "views/aicanimastateview.h"
#include "views/aicanimatilesview.h"
#include "views/aicimageview.h"
#include "views/aicscaleview.h"
#include "views/aicrtpview.h"
#include "views/aicdashboardview.h"
#include "views/aicvideoview.h"
#ifdef QTLAUNCHER_WIFI_MANAGER
#include "views/aicconfigview.h"
#include "wifi/aicwifithread.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <wifimanager.h>
#endif

#define M_DEMO_MODE_NORMAL 0
#define M_DEMO_MODE_RTP_WINDOW 1

class MainWindow : public AiCBaseWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);

private:
    void initVariable();
    void initWindow();
    void initCentralButtons();
    void initWiFiManager();
    void slideToRight();
    void slideToLeft();
    void initSmallWindow(int width, int height);
    void switchView(QWidget *newWidget);
    void checkMenuVideoStatus();


protected slots:
    void onMenuClicked();
    void onHomeClicked();
    void onBackClicked();
    void onCleanClicked();

    void onBtnTimeClicked();
    void onBtnCPUClicked();
    void onBtnMusicClicked();
    void onBtnVideoClicked();
    void onBtnRTPClicked();
    void onBtnImagesClicked();
    void onBtnScaleClicked();
    void onBtnConfigClicked();
    void onBtnAnim1Clicked();
    void onBtnAnim2Clicked();
    void onBtnAnim3Clicked();
    void onVideoTimeout();

private:
    QStackedWidget *mStackedWidget;

    QHBoxLayout *mCentralLayout;
    AiCNavigationBar *mNavBar;
    AiCStatusBar *mStatusBar;

    AiCDateTimeButton *mBtnTime;
    AiCDesktopButton *mBtnCPU;
    AiCDesktopButton *mBtnMusic;
    AiCDesktopButton *mBtnVideo;
    AiCDesktopButton *mBtnRTP;
    AiCDesktopButton *mBtnImage;
    AiCDesktopButton *mBtnScale;
    AiCDesktopButton *mBtnConfig;
    AiCDesktopButton *mBtnAnim1;
    AiCDesktopButton *mBtnAnim2;
    AiCDesktopButton *mBtnAnim3;

private:
    QPoint mStartPoint;
    QPushButton *mCleanButton;
    AiCCentralView *mCentralView;
    AiCBriefView *mBriefView;
    AiCRtpView *mRtpView;
    AiCVideoView *mVideoView;
    QTimer *mVideoTimer;

    int mDemoMode;
#ifdef QTLAUNCHER_WIFI_MANAGER
    AiCConfigView *mConfigView;
    wifimanager_cb_t mwifimanager_cb_t;
#endif
};

#endif // MAINWINDOW_H
