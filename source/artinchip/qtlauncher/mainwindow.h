#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QPoint>

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
    void slideToRight();
    void slideToLeft();
    void initSmallWindow(int width, int height);
    void switchView(QWidget *newWidget);


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

    int mDemoMode;

};

#endif // MAINWINDOW_H
