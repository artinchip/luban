#ifndef AICANIMAIMAGEVIEW_H
#define AICANIMAIMAGEVIEW_H

#include <QGraphicsView>
#include <QWidget>
#include <QLabel>
#include <QList>
#include <QPaintEvent>
#include <QPoint>
#include <QPropertyAnimation>
#include <QHBoxLayout>

class AiCAnimaImageView : public QWidget
{
public:
    AiCAnimaImageView(QSize size, QWidget *parent = NULL);
    ~AiCAnimaImageView();

protected:
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);

private:
    void initView(int width, int height);
    void startAnimation();
    int randomValue(int max);

private:
    int ANIMATION_ITEMS;
    int ANIMATION_DURATIONS;

private:
    QList<QLabel *> mLabelList;
    QList<QPropertyAnimation *> mAnimList;
    QList<QPoint> mPointList;
};

#endif // AICANIMAIMAGEVIEW_H
