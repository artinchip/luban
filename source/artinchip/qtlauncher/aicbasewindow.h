#ifndef AICBASEWINDOW_H
#define AICBASEWINDOW_H

#include <QWidget>
#include <QHBoxLayout>
#include <QPaintEvent>

#include "widgets/aicnavigationbar.h"
#include "widgets/aicstatusbar.h"

class AiCBaseWindow : public QWidget
{
    Q_OBJECT
public:
    explicit AiCBaseWindow(QWidget *parent = 0, Qt::WindowFlags flags = Qt::FramelessWindowHint);
    explicit AiCBaseWindow(QPixmap background, QWidget *parent = 0, Qt::WindowFlags flags = Qt::FramelessWindowHint);
    virtual ~AiCBaseWindow();

    AiCNavigationBar *navigationBar() {return mNavBar;}
    AiCStatusBar *statusBar() {return mStatusBar;}
    QHBoxLayout *centralLayout() {return mCentralLayout;}

   protected:
       void paintEvent(QPaintEvent *event);
       void initWindow();

   signals:
       void quit();

   protected slots :
       virtual void onMenuClicked();
       virtual void onBackClicked();
       virtual void onHomeClicked();

   public slots:

   private:
       AiCNavigationBar *mNavBar;
       QHBoxLayout *mCentralLayout;
       AiCStatusBar *mStatusBar;
       QVBoxLayout *mLayout;
       QPixmap mBGPixmap;
};
#endif // AICBASEWIDGET_H
