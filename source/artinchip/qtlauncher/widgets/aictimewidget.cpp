#include "aictimewidget.h"

#include <QTimer>
#include <QTime>
#include <QPainter>
#include <QDebug>

AiCTimeWidget::AiCTimeWidget(QWidget *parent) : QWidget(parent)
{
    setFixedSize(210,210);
    initWidget();
}

AiCTimeWidget::AiCTimeWidget(QSize size, QWidget *parent) : QWidget(parent)
{
    setFixedSize(size);
    initWidget();
}

void AiCTimeWidget::initWidget()
{
    mAutoMode = false;
    mTimer = new QTimer(this);
    connect(mTimer, SIGNAL(timeout()), this, SLOT(update()));
}

void AiCTimeWidget::setAutoMode(bool autoMode)
{
    if(autoMode){
        mTimer->start(1000);
    }
    else{
        mTimer->stop();
    }
    mAutoMode = autoMode;
}

void AiCTimeWidget::setShowStyle(int style)
{
    mStyle = style;
    if(style != 0){
        if(style == 1){
            mClockPixmap = QPixmap(":/resources/clock/clock.png");
        }
        else{
            mClockPixmap = QPixmap(":/resources/clock/clock-night.png");
        }
        mCenterPixmap = QPixmap(":/resources/clock/center.png");
        mHourPixmap = QPixmap(":/resources/clock/hour.png");
        mMinutePixmap = QPixmap(":/resources/clock/minute.png");
        mSecondPixmap = QPixmap(":/resources/clock/second.png");
    }
}

void AiCTimeWidget::paintManually()
{
    static const QPoint hourHand[3] = {
        QPoint(7, 8),
        QPoint(-7, 8),
        QPoint(0, -40)
    };
    static const QPoint minuteHand[3] = {
        QPoint(7, 8),
        QPoint(-7, 8),
        QPoint(0, -70)
    };

    static const QPoint secondHand[3] = {
        QPoint(3, 3),
        QPoint(-3, 3),
        QPoint(0, -90)
    };

    QPainter painter(this);
    QColor hourColor(127, 0, 127);
    QColor minuteColor(0, 127, 127, 191);
    QColor secondColor(127, 127, 0, 191);

    int side = qMin(width(), height());
    QTime time = QTime::currentTime();

    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(width() / 2, height() / 2);
    painter.scale(side / 200.0, side / 200.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(hourColor);

    painter.save();
    painter.rotate(30.0 * ((time.hour() + time.minute() / 60.0)));
    painter.drawConvexPolygon(hourHand, 3);
    painter.restore();

    painter.setPen(hourColor);

    for (int i = 0; i < 12; ++i) {
        painter.drawLine(88, 0, 96, 0);
        painter.rotate(30.0);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(minuteColor);

    painter.save();
    painter.rotate(6.0 * (time.minute() + time.second() / 60.0));
    painter.drawConvexPolygon(minuteHand, 3);
    painter.restore();

    painter.setPen(minuteColor);

    for (int j = 0; j < 60; ++j) {
        if ((j % 5) != 0){
            painter.drawLine(92, 0, 96, 0);
            painter.rotate(6.0);
	}
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(secondColor);

    painter.save();
    painter.rotate(6.0 * time.second());
    painter.drawConvexPolygon(secondHand, 3);
    painter.restore();
}

void AiCTimeWidget::paintPng()
{
    QPainter painter(this);
    QTime time = QTime::currentTime();

    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(width() / 2, height() / 2);

    painter.drawPixmap(-100, -100, mClockPixmap);
    painter.drawPixmap(- 11, -11, mCenterPixmap);

    painter.save();
    painter.rotate(180 + (30.0 * ((time.hour() - 12 + time.minute() / 60.0))));
    painter.drawPixmap(-7.5, -7.5 , mHourPixmap);
    painter.restore();

    painter.save();
    painter.rotate(180 + (6.0 * (time.minute() + time.second() / 60.0)));
    painter.drawPixmap(-6.5, -6.5, mMinutePixmap);
    painter.restore();

    painter.save();
    painter.rotate(180 + (6.0 * time.second()));
    painter.drawPixmap(-2.5, -2.5, mSecondPixmap);
    painter.restore();

}

void AiCTimeWidget::paintEvent(QPaintEvent *event)
{
    if(mStyle == 0){
        paintManually();
    }
    else{
        paintPng();
    }
    QWidget::paintEvent(event);
}

AiCTimeWidget::~AiCTimeWidget()
{
    mTimer->stop();
    delete mTimer;
}
