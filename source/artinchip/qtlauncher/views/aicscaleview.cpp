#include "aicscaleview.h"
#include "utils/aicconsts.h"

AiCScaleView::AiCScaleView(QSize size, QWidget *parent) : QWidget(parent)
{
    this->setFixedSize(size);
    mScaleAnimation1 = NULL;
    mScaleAnimation2 = NULL;
#ifdef QTLAUNCHER_SMALL_MEMORY
    mCurrentPixmap = QPixmap(":/resources/images/image1.jpeg");
#else
    int width = size.width();
    int height = size.height();
    SWITCH_DURATION = 5000;
    mCurrentDrawImageIndx = 0;

    mScaleAnimation1 = new QPropertyAnimation(this, "geometry");
    mScaleAnimation1->setStartValue(QRect(0,0,width,height));
    mScaleAnimation1->setEndValue(QRect(width/3,height/3,30,30));
    mScaleAnimation1->setDuration(SWITCH_DURATION);

    mScaleAnimation2 = new QPropertyAnimation(this, "geometry");
    mScaleAnimation2->setStartValue(QRect(width/3,height/3,30,30));
    mScaleAnimation2->setEndValue(QRect(0,0,width,height));
    mScaleAnimation2->setDuration(SWITCH_DURATION);

    connect(&mImageChangeTimer, SIGNAL(timeout()), this, SLOT(onImageChangeTimeout()));
    initImages();
    startPlay();
#endif
}

void AiCScaleView::initImages()
{
    addImage(":/resources/images/image1.jpeg");
    addImage(":/resources/images/image2.jpeg");
    addImage(":/resources/images/image3.jpeg");
    addImage(":/resources/images/image4.jpeg");
    addImage(":/resources/images/image5.jpeg");
    addImage(":/resources/images/image6.jpeg");
}

void AiCScaleView::setImageList(QStringList imageFileNameList)
{
    mImageFileNameList = imageFileNameList;
}

void AiCScaleView::addImage(QString imageFileName)
{
    mImageFileNameList.append(imageFileName);
}

void AiCScaleView::startPlay()
{
    if (mImageFileNameList.count() > 1){
        mCurrentPixmap = QPixmap(mImageFileNameList.at(0));
        mImageChangeTimer.start(SWITCH_DURATION);

        if(mCurrentDrawImageIndx % 2 == 0){
                mScaleAnimation1->start();
                mCurrentZoom = 1;
        }
        else{
            mScaleAnimation2->start();
            mCurrentZoom = 2;
        }
    }

    update();
}

void AiCScaleView::onImageChangeTimeout()
{
    int total = mImageFileNameList.count();
    mCurrentPixmap = QPixmap(mImageFileNameList.at(mCurrentDrawImageIndx));
    mCurrentDrawImageIndx = (mCurrentDrawImageIndx + 1) % total;

    if(mCurrentDrawImageIndx % 2 == 0){
        mCurrentZoom = 1;
        mScaleAnimation1->start();
    }
    else{
        mCurrentZoom = 2;
        mScaleAnimation2->start();
    }
}

void AiCScaleView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
#ifdef QTLAUNCHER_SMALL_MEMORY
    QRect drawrect = QRect(this->rect().width()/4, this->rect().height()/4,this->rect().width()/2, this->rect().height()/2);
    painter.drawPixmap(drawrect, mCurrentPixmap.scaled(drawrect.width(),drawrect.height(),Qt::KeepAspectRatio));
#else
    if(mCurrentZoom == 1){
        QRect image1Rect = mScaleAnimation1->currentValue().toRect();
        painter.drawPixmap(image1Rect, mCurrentPixmap.scaled(image1Rect.width(),image1Rect.height(),Qt::KeepAspectRatio));
    }
    else{
        QRect image2Rect = mScaleAnimation2->currentValue().toRect();
        painter.drawPixmap(image2Rect, mCurrentPixmap.scaled(image2Rect.width(),image2Rect.height(),Qt::KeepAspectRatio));
    }
#endif
}

void AiCScaleView::mousePressEvent(QMouseEvent* event)
{
    return QWidget::mousePressEvent(event);
}

AiCScaleView::~AiCScaleView()
{
    if(mScaleAnimation1)
        delete mScaleAnimation1;
    if(mScaleAnimation2)
        delete mScaleAnimation2;
}
