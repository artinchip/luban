#include "aicdatetimebutton.h"
#include "aictimewidget.h"

#include <QPainter>
#include <QTimer>
#include <QTime>
#include <QDate>
#include <QDebug>

AiCDateTimeButton::AiCDateTimeButton(QWidget *parent) : QPushButton(parent)
{
    mSize = QSize(215,330);
    initButton();
}

AiCDateTimeButton::AiCDateTimeButton(QSize size, QWidget *parent) : QPushButton(parent)
{
    mSize = size;
    initButton();
}

void AiCDateTimeButton::initButton()
{
    mLayout = new QVBoxLayout(this);
    mLayout->setContentsMargins(0, 5 , 0, 5);

    mDateLabel = new QLabel();
    mDateLabel->setFixedSize(200,40);
    mDateLabel->setAlignment(Qt::AlignCenter);
    mDateLabel->setStyleSheet("color: #000000;font-size:22px;");
    mLayout->addWidget(mDateLabel, 0, Qt::AlignCenter);

    /*mTimeLabel = new QLabel();
    mTimeLabel->setFixedSize(200,60);
    mTimeLabel->setAlignment(Qt::AlignCenter);
    mTimeLabel->setStyleSheet("color: #000000;font-size:42px;");
    mTimeLabel->setText("10:10:33");
    mLayout->addWidget(mTimeLabel, 0, Qt::AlignCenter);*/

    mTimeNum = new QLCDNumber();
    mTimeNum->setFixedSize(200,50);
    mTimeNum->setDigitCount(8);
    mTimeNum->setSegmentStyle(QLCDNumber::Flat);
    mTimeNum->setStyleSheet("background-color:#fff; color: #009230; font-size:18px;");
    mLayout->addWidget(mTimeNum, 0, Qt::AlignCenter);

    mTimeWidget = new AiCTimeWidget();
    mTimeWidget->setFixedSize(210, 210);\
    mTimeWidget->setShowStyle(1);
    mLayout->addWidget(mTimeWidget, 0, Qt::AlignCenter);
    setFixedSize(mSize);

    mTimer = new QTimer(this);
    connect(mTimer, SIGNAL(timeout()), this, SLOT(onCountTimeOut()));
    onCountTimeOut();
    mTimer->start(1000);
}

void AiCDateTimeButton::onCountTimeOut()
{
    QTime time = QTime::currentTime();
    mTimeNum->display(time.toString("hh:mm:ss"));

    QDate date = QDate::currentDate();
    mDateLabel->setText(date.toString("yyyy-MM-dd ddd"));

    mTimeWidget->update();
}

void AiCDateTimeButton::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(this->rect(),QColor(255,255,255));
    QWidget::paintEvent(event);
}

AiCDateTimeButton::~AiCDateTimeButton()
{
    mTimer->stop();
    delete mTimer;
    delete mDateLabel;
    delete mTimeLabel;
    delete mTimeWidget;
    delete mLayout;
}
