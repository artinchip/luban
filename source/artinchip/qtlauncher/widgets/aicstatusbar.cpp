#include "aicstatusbar.h"
#include "utils/aicconsts.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QPainter>
#include <QLabel>
#include <QIcon>
#include <QTime>
#include <QDebug>

AiCStatusBar::AiCStatusBar(QWidget *parent) : QWidget(parent)
{
    mSize = QSize(AIC_STATUS_BAR_WIDTH, AIC_STATUS_BAR_HEIGHT);
    initBar();
}

AiCStatusBar::AiCStatusBar(QSize size, QWidget *parent) : QWidget(parent)
{
    mSize = size;
    initBar();
}

void AiCStatusBar::initBar()
{
    mBGPixmap = QPixmap(":/resources/statusbar/bg.png");
    mWiFiOnPixmap = QPixmap(":/resources/statusbar/wifi_on.png");
    mWiFiOffPixmap = QPixmap(":/resources/statusbar/wifi_on.png");

    mLayout = new QHBoxLayout(this);
    mLayout->setSpacing(5);
    mLayout->setContentsMargins(10,0,10,0);

    QFont font = this->font();
    font.setPointSize(FONT_NORMAL);

    mTimeLabel = new QLabel(this);
    mTimeLabel->setFont(font);
    mLayout->addWidget(mTimeLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);

    mTitleLabel = new QLabel(this);
    mTitleLabel->setFont(font);
    mTitleLabel->setAlignment(Qt::AlignCenter);
    mTitleLabel->setFixedWidth(mSize.width() - mSize.height() * 5);
    mLayout->addWidget(mTitleLabel, 0, Qt::AlignCenter);

    mWifiStatus = WIFI_ON;
    mWiFiLabel = new QLabel(this);
    mLayout->addWidget(mWiFiLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

    updateTime();
    updateTitle("Music Title");
    updateWiFi(mWifiStatus);

    setFixedSize(mSize);
    this->setLayout(mLayout);

    setupTimer();
}

void AiCStatusBar::updateTitle(QString strTitle)
{
    mTitleLabel->setText(strTitle);
}

void AiCStatusBar::updateWiFi(WifiStatusType status)
{
    if (status == WIFI_ON) {
        mWiFiLabel->setPixmap(mWiFiOnPixmap);
    } else {
        mWiFiLabel->setPixmap(mWiFiOffPixmap);
    }

    mWifiStatus = status;
}

void AiCStatusBar::updateTime()
{
    QTime time = QTime::currentTime();
    QString strTime = time.toString("hh:mm");
    mTimeLabel->setText(strTime);
}

void AiCStatusBar::setupTimer()
{
    mTimer = new QTimer(this);
    connect(mTimer, SIGNAL(timeout()), this, SLOT(onTimeOut()));
    mTimer->start(60 * 1000);
}

void AiCStatusBar::onTimeOut()
{
    updateTime();
}

void AiCStatusBar::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, mBGPixmap.scaled(size()));
    QWidget::paintEvent(event);
}

AiCStatusBar::~AiCStatusBar()
{
    delete mWiFiLabel;
    delete mTimeLabel;
    delete mLayout;
}
