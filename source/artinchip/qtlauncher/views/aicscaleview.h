#ifndef AICSCALEVIEW_H
#define AICSCALEVIEW_H

#include <QWidget>
#include <QtGui>

class AiCScaleView : public QWidget
{
    Q_OBJECT
public:
    explicit AiCScaleView(QSize size, QWidget *parent = 0);
    ~AiCScaleView();

    void setImageList(QStringList imageFileNameList);
    void addImage(QString imageFileName);
    void startPlay();

private:
    void initImages();
    void initChangeImageButton();
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent* event);
public slots:
    void onImageChangeTimeout();

private:
    int SWITCH_DURATION;
    int mCurrentZoom;
    QList<QString> mImageFileNameList;

    QTimer mImageChangeTimer;
    int mCurrentDrawImageIndx;

    QPixmap mCurrentPixmap;
    QPropertyAnimation* mScaleAnimation1;
    QPropertyAnimation* mScaleAnimation2;
};

#endif // AICSCALEVIEW_H
