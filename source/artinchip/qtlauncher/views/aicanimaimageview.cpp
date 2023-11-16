#include "aicanimaimageview.h"

#include <QDebug>

AiCAnimaImageView::AiCAnimaImageView(QSize size, QWidget *parent) : QWidget(parent)
{
    initView(size.width(), size.height());
}

void AiCAnimaImageView::initView(int width, int height)
{
    ANIMATION_ITEMS = 5;
    ANIMATION_DURATIONS = 2000;

    QList<QPixmap> pixmap;
    pixmap.append(QPixmap(":/resources/animation/ic1.jpg"));
    pixmap.append(QPixmap(":/resources/animation/ic2.jpg"));
    pixmap.append(QPixmap(":/resources/animation/ic3.jpg"));
    pixmap.append(QPixmap(":/resources/animation/ic4.jpg"));
    pixmap.append(QPixmap(":/resources/animation/ic5.jpg"));

    mPointList.append(QPoint(randomValue(width),randomValue(height)));
    for(int i = 0; i < ANIMATION_ITEMS; i ++){
        QLabel *label = new QLabel(this);
        label->setPixmap(pixmap.at(i));
        label->show();
        mAnimList.append(new QPropertyAnimation(label,"pos"));
        mPointList.append(QPoint(randomValue(width),randomValue(480)));
        mLabelList.append(label);
    }

    startAnimation();
}

int AiCAnimaImageView::randomValue(int max)
{
    return (qrand() % max);
}

void AiCAnimaImageView::startAnimation()
{
    QPropertyAnimation *animation;
    for(int i = 0; i < ANIMATION_ITEMS; i ++){
        animation = mAnimList.at(i);
        animation->setDuration(ANIMATION_DURATIONS);
        animation->setStartValue(mPointList.at(i));
        animation->setEndValue(mPointList.at(i + 1));
        animation->setEasingCurve(QEasingCurve::OutBounce);
        animation->start();
    }
}

void AiCAnimaImageView::mousePressEvent(QMouseEvent *event)
{
    QWidget::mouseMoveEvent(event);
}

void AiCAnimaImageView::mouseReleaseEvent(QMouseEvent *event)
{
    mPointList.removeAt(0);
    mPointList.append(event->pos());
    startAnimation();
    QWidget::mouseReleaseEvent(event);
}

AiCAnimaImageView::~AiCAnimaImageView()
{
    for(int i = 0; i < ANIMATION_ITEMS; i ++){
        delete mLabelList.at(i);
        delete mAnimList.at(i);
    }
}
