/*
 * Copyright (C) 2024-2025 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 *         Huahui Mai <huahui.mai@artinchip.com>
 */

#include "mainwindow.h"
#include "aicbasewindow.h"
#include "utils/aicconsts.h"

#include <QDebug>
#include <QDesktopWidget>
#include <QApplication>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : AiCBaseWindow(QPixmap(":/resources/background.png"), parent)
{
    int height, width;

    QDesktopWidget *pd = QApplication::desktop();
    QRect screenRect = pd->screenGeometry();

    width = screenRect.width();
    height = screenRect.height();

    initVariable();
    setFixedSize(QSize(width, height));
    if (width >= AIC_DEFAULT_WIDTH && height >= AIC_DEFAULT_HEIGHT) {
        setFixedSize(AIC_DEFAULT_WIDTH, AIC_DEFAULT_HEIGHT);
        initWindow();
        mDemoMode = M_DEMO_MODE_NORMAL;
    } else {
        setFixedSize(width, height);
        initSmallWindow(width, height);
        mDemoMode = M_DEMO_MODE_RTP_WINDOW;
    }
}

void MainWindow::initVariable()
{

}

void MainWindow::initWindow()
{
    mCentralLayout = centralLayout();

    mStatusBar = statusBar();
    mNavBar = navigationBar();

    mStackedWidget = new QStackedWidget;
    mCentralLayout->addWidget(mStackedWidget);

    mBriefView = new AiCBriefView(QSize(AIC_CENTRAL_VIEW_WIDTH,AIC_CENTRAL_VIEW_HEIGHT));

    mCentralView = new AiCCentralView();
    initCentralButtons();
    initWiFiManager();

    mStackedWidget->addWidget(mBriefView);
    mStackedWidget->addWidget(mCentralView);
    mStackedWidget->addWidget(new AiCDateTimeView(QSize(AIC_CENTRAL_VIEW_WIDTH,AIC_CENTRAL_VIEW_HEIGHT)));

    mStackedWidget->setCurrentIndex(1);
}
void MainWindow::initSmallWindow(int width, int height)
{
    mCentralLayout = centralLayout();

    mCleanButton = NULL;
    mCleanButton = new QPushButton;
    mCleanButton->setText("Clean");
    mCleanButton->setFixedSize(QSize(width/8, height));
    connect(mCleanButton, SIGNAL(clicked()), SLOT(onCleanClicked()));
    mCentralLayout->addWidget(mCleanButton);

    mRtpView = new AiCRtpView(QSize(width,height));
    mCentralLayout->addWidget(mRtpView);

    mNavBar = navigationBar();
    mNavBar->hide();
}

void MainWindow::onCleanClicked()
{
    if(mRtpView)
        mRtpView->cleanView();
}

void MainWindow::initCentralButtons()
{
    int width = AIC_CENTRAL_BUTTON_WIDTH;
    int height = AIC_CENTRAL_BUTTON_HEIGHT;
    int heightx2 = AIC_CENTRAL_BUTTON_HEIGHT * 2 + AIC_CENTRAL_BUTTON_YMARGIN * 2;

    //int row, int column, int rowSpan, int columnSpan
    mBtnTime = new AiCDateTimeButton(QSize(width,heightx2));
    connect(mBtnTime, SIGNAL(clicked()), SLOT(onBtnTimeClicked()));
    mCentralView->addButtonWidget(mBtnTime,0,0,2,1);

    mBtnCPU = new AiCDesktopButton(QSize(width,height),AiCDesktopButton::BG_DEEP_RED);
    mBtnCPU->setParameters(QPixmap(":/resources/central/cpu.png"),"GE");
    connect(mBtnCPU, SIGNAL(clicked()), SLOT(onBtnCPUClicked()));
    mCentralView->addButtonWidget(mBtnCPU,2,0,1,1);

    mBtnMusic = new AiCDesktopButton(QSize(width,height),AiCDesktopButton::BG_LIGHT_RED);
    mBtnMusic->setParameters(QPixmap(":/resources/central/music.png"),"Music");
    connect(mBtnMusic, SIGNAL(clicked()), SLOT(onBtnMusicClicked()));
    mCentralView->addButtonWidget(mBtnMusic,0,1,1,1);

    mBtnVideo = new AiCDesktopButton(QSize(width,height),AiCDesktopButton::BG_DEEP_GREEN);
    mBtnVideo->setParameters(QPixmap(":/resources/central/video.png"),"Video");
    connect(mBtnVideo, SIGNAL(clicked()), SLOT(onBtnVideoClicked()));
    mCentralView->addButtonWidget(mBtnVideo,0,2,1,1);
    mVideoTimer = NULL;

    mBtnRTP = new AiCDesktopButton(QSize(width,height),AiCDesktopButton::BG_LIGHT_GREEN);
    mBtnRTP->setParameters(QPixmap(":/resources/central/rtp.png"),"RTP");
    connect(mBtnRTP, SIGNAL(clicked()), SLOT(onBtnRTPClicked()));
    mCentralView->addButtonWidget(mBtnRTP,0,3,1,1);

    mBtnImage = new AiCDesktopButton(QSize(width,height),AiCDesktopButton::BG_DEEP_PURPLE);
    mBtnImage->setParameters(QPixmap(":/resources/central/image1.png"),"Image");
    connect(mBtnImage, SIGNAL(clicked()), SLOT(onBtnImagesClicked()));
    mCentralView->addButtonWidget(mBtnImage,1,1,1,1);

    mBtnScale = new AiCDesktopButton(QSize(width,height),AiCDesktopButton::BG_LIGHT_PURPLE);
    mBtnScale->setParameters(QPixmap(":/resources/central/image2.png"),"Scale");
    connect(mBtnScale, SIGNAL(clicked()), SLOT(onBtnScaleClicked()));
    mCentralView->addButtonWidget(mBtnScale,1,2,1,1);

    mBtnAnim1 = new AiCDesktopButton(QSize(width,height),AiCDesktopButton::BG_DEEP_BLUE);
    mBtnAnim1->setParameters(QPixmap(":/resources/central/animation1.png"),"Sample");
    connect(mBtnAnim1, SIGNAL(clicked()), SLOT(onBtnAnim1Clicked()));
    mCentralView->addButtonWidget(mBtnAnim1,1,3,1,1);

    mBtnAnim2 = new AiCDesktopButton(QSize(width,height),AiCDesktopButton::BG_DEEP_BLUE);
    mBtnAnim2->setParameters(QPixmap(":/resources/central/animation2.png"),"Sample");
    connect(mBtnAnim2, SIGNAL(clicked()), SLOT(onBtnAnim2Clicked()));
    mCentralView->addButtonWidget(mBtnAnim2,2,1,1,1);

    mBtnAnim3 = new AiCDesktopButton(QSize(width,height),AiCDesktopButton::BG_LIGHT_BLUE);
    mBtnAnim3->setParameters(QPixmap(":/resources/central/animation3.png"),"Sample");
    connect(mBtnAnim3, SIGNAL(clicked()), SLOT(onBtnAnim3Clicked()));
    mCentralView->addButtonWidget(mBtnAnim3,2,2,1,1);

    mBtnConfig = new AiCDesktopButton(QSize(width,height),AiCDesktopButton::BG_DEEP_PURPLE);
    mBtnConfig->setParameters(QPixmap(":/resources/central/setting.png"),"Settings");
    connect(mBtnConfig, SIGNAL(clicked()), SLOT(onBtnConfigClicked()));
    mCentralView->addButtonWidget(mBtnConfig,2,3,1,1);
}

#ifdef QTLAUNCHER_WIFI_MANAGER
static void scanResultCallback(char *result)
{
    int ret;
    key_t key = ftok("/tmp", 777);
    int msgid = msgget(key, IPC_CREAT | 0666);

    struct wifiMsg msg;
    msg.mtype = MSG_TYPE;
    memcpy(msg.mtext, result, strlen(result));

    ret = msgsnd(msgid, &msg, MAX_SIZE, 0);
    if (ret)
        qDebug("msgsnd failed\n");
}

void statChangeCallback(wifistate_t stat, wifimanager_disconn_reason_t reason)
{
    int ret;
    key_t key = ftok("/tmp", 777);
    int msgid = msgget(key, IPC_CREAT | 0666);

    struct wifiMsg msg;
    msg.mtype = MSG_TYPE;
    char *data = msg.mtext;

    data[0] = 0xEE;
    data[1] = (char)stat;
    data[2] = (char)reason;

    ret = msgsnd(msgid, &msg, MAX_SIZE, 0);
    if (ret)
        qDebug("msgsnd Change failed\n");
}

void MainWindow::initWiFiManager()
{
    QString fileName = AIC_WIFI_CONFIG_FILE;

    if (!QFile::exists(fileName)) {
        QSettings *setting = new QSettings(fileName, QSettings::IniFormat);

        setting->setValue("wifimanager", 0);
        setting->sync();

        delete setting;
    }

    mwifimanager_cb_t.scan_result_cb = scanResultCallback;
    mwifimanager_cb_t.stat_change_cb = statChangeCallback;
    wifimanager_init(&mwifimanager_cb_t);
}
#else
void MainWindow::initWiFiManager()
{
    qDebug() << __func__;
}
#endif

void MainWindow::slideToRight()
{
    int current = mStackedWidget->currentIndex();
    if (current == 2)
        mStackedWidget->setCurrentIndex(1);
    else if (current == 1)
        mStackedWidget->setCurrentIndex(0);
}

void MainWindow::slideToLeft()
{
    int current = mStackedWidget->currentIndex();
    if (current == 0)
        mStackedWidget->setCurrentIndex(1);
    else if (current == 1)
        mStackedWidget->setCurrentIndex(2);
}

void MainWindow:: onBtnTimeClicked()
{
    QWidget *currentWidget = new AiCDateTimeView(QSize(AIC_CENTRAL_VIEW_WIDTH,AIC_CENTRAL_VIEW_HEIGHT));
    switchView(currentWidget);
}

void MainWindow:: onBtnCPUClicked()
{
    QWidget *currentWidget = new AiCDashBoardView(QSize(AIC_CENTRAL_VIEW_WIDTH,AIC_CENTRAL_VIEW_HEIGHT));
    switchView(currentWidget);
}

void MainWindow:: onBtnMusicClicked()
{
    qDebug() << __func__;
}

#ifdef QTLAUNCHER_GE_SUPPORT
void MainWindow:: onBtnVideoClicked()
{
    QWidget *currentWidget = new AiCVideoView(QSize(AIC_CENTRAL_VIEW_WIDTH,AIC_CENTRAL_VIEW_HEIGHT));

    switchView(currentWidget);
    mVideoView = (AiCVideoView *)currentWidget;

    mNavBar->disableMenu(true);

    mVideoTimer = new QTimer(this);
    mVideoTimer->setSingleShot(true);
    mVideoTimer->setInterval(200);
    connect(mVideoTimer, SIGNAL(timeout()), SLOT(onVideoTimeout()));
    mVideoTimer->start();
}

void MainWindow::onVideoTimeout()
{
    QWidget *curWidget = mStackedWidget->currentWidget();
    AiCVideoView *videoView = (AiCVideoView *)curWidget == mVideoView ? mVideoView : NULL;

    if (videoView) {
        videoView->geBgFill();
        videoView->geBtnBlt();
    }

    mVideoTimer->deleteLater();
    mVideoTimer = NULL;
}
#else /* QTLAUNCHER_GE_SUPPORT */
void MainWindow:: onBtnVideoClicked()
{
    qDebug() << __func__;

    mVideoTimer = new QTimer(this);
    mVideoTimer->setSingleShot(true);
    mVideoTimer->setInterval(20);
    connect(mVideoTimer, SIGNAL(timeout()), SLOT(onVideoTimeout()));
    mVideoTimer->start();
}

void MainWindow::onVideoTimeout()
{
    qDebug() << __func__;

    mVideoTimer->deleteLater();
    mVideoTimer = NULL;
}
#endif

void MainWindow:: onBtnRTPClicked()
{
    QWidget *currentWidget = new AiCRtpView(QSize(AIC_CENTRAL_VIEW_WIDTH,AIC_CENTRAL_VIEW_HEIGHT));
    switchView(currentWidget);
    mDemoMode = M_DEMO_MODE_RTP_WINDOW;
}

void MainWindow:: onBtnImagesClicked()
{
    QWidget *currentWidget = new AiCImageView(QSize(AIC_CENTRAL_VIEW_WIDTH,AIC_CENTRAL_VIEW_HEIGHT));
    switchView(currentWidget);

    qDebug() << __func__;
}


void MainWindow:: onBtnScaleClicked()
{
    QWidget *currentWidget = new AiCScaleView(QSize(AIC_CENTRAL_VIEW_WIDTH,AIC_CENTRAL_VIEW_HEIGHT));
    switchView(currentWidget);
    qDebug() << __func__;
}

void MainWindow:: onBtnConfigClicked()
{
#ifdef QTLAUNCHER_WIFI_MANAGER
    QWidget *currentWidget = new AiCConfigView(QSize(AIC_CENTRAL_VIEW_WIDTH,AIC_CENTRAL_VIEW_HEIGHT));
    switchView(currentWidget);
#endif
    qDebug() << __func__;
}

void MainWindow:: onBtnAnim1Clicked()
{
    QWidget *currentWidget = new AiCAnimaTilesView(QSize(AIC_CENTRAL_VIEW_WIDTH,AIC_CENTRAL_VIEW_HEIGHT));
    switchView(currentWidget);
}

void MainWindow:: onBtnAnim2Clicked()
{
    QWidget *currentWidget = new AiCAnimaImageView(QSize(AIC_CENTRAL_VIEW_WIDTH,AIC_CENTRAL_VIEW_HEIGHT));
    switchView(currentWidget);
    qDebug() << __func__;

}

void MainWindow:: onBtnAnim3Clicked()
{
    QWidget *currentWidget = new AiCAnimaStateView(QSize(AIC_CENTRAL_VIEW_WIDTH,AIC_CENTRAL_VIEW_HEIGHT));
    switchView(currentWidget);
}

void MainWindow:: switchView(QWidget *newWidget)
{
    mDemoMode = M_DEMO_MODE_NORMAL;
    QWidget *oldWidget = mStackedWidget->widget(2);
    mStackedWidget->removeWidget(oldWidget);
    mStackedWidget->insertWidget(2,newWidget);
    mStackedWidget->setCurrentIndex(2);
    mNavBar->disableMenu(false);

    if (oldWidget != NULL) {
        delete oldWidget;
        oldWidget = NULL;
    }
}

void MainWindow::checkMenuVideoStatus()
{
    QWidget *oldWidget = mStackedWidget->currentWidget();

    if (oldWidget == (QWidget *)mVideoView) {
        mVideoView->videoStop();
        mNavBar->disableMenu(false);
    }
}

void MainWindow::onMenuClicked()
{
    qDebug() << __FILE__ << __func__ << __LINE__;
}

void MainWindow::onHomeClicked()
{
    if (mVideoTimer && mVideoTimer->isActive())
        mVideoTimer->stop();

    checkMenuVideoStatus();

    mStackedWidget->setCurrentIndex(1);
}

void MainWindow::onBackClicked()
{
    if (mVideoTimer && mVideoTimer->isActive())
        mVideoTimer->stop();

    checkMenuVideoStatus();

    mStackedWidget->setCurrentIndex(1);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if(mDemoMode == M_DEMO_MODE_RTP_WINDOW)
        return;

    mStartPoint = event->pos();
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if(mDemoMode == M_DEMO_MODE_RTP_WINDOW)
        return;

    QWidget::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if(mDemoMode == M_DEMO_MODE_RTP_WINDOW)
        return;

    QPoint distance = event->pos() - mStartPoint;
    if ((abs(distance.x()) >= width()/8) && (distance.x() > 0)) {
        slideToRight();

    } else if ((abs(distance.x()) >= width()/8) && (distance.x() < 0)) {
        slideToLeft();
    }
}


MainWindow::~MainWindow()
{
    if(mCentralView != NULL)
       delete mCentralView;

    if(mRtpView != NULL)
        delete mRtpView;

    if(mBriefView != NULL)
       delete mBriefView;

}
