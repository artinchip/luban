/*
 * Copyright (C) 2025 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors: Huahui Mai <huahui.mai@artinchip.com>
 */

#include "aicconfigview.h"
#include "utils/aictypes.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

static const QString qss = "                               \
                            QLineEdit {                    \
                                border-style: none;        \
                                padding: 3px;              \
                                border-radius: 5px;        \
                                border: 1px solid #dce5ec; \
                                font-size: 30px;           \
                            }                              \
                            ";

AiCConfigView::AiCConfigView(QSize size, QWidget *parent) : QWidget(parent)
{
    initView(size.width(), size.height());
}

void AiCConfigView::initView(int width, int height)
{
    wifi_status_t status;

    (void)width;
    (void)height;

    mLayout = new QGridLayout(this);
    mScrollArea = new QScrollArea(this);

    mScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    mScrollArea->setGeometry(QRect(0, 0, 800, 200));

    mWifiButton = new QPushButton(this);
    mWifiButton->setFlat(true);
    mWifiButton->setFixedSize(150, 50);

    QFont font;
    font.setPixelSize(20);
    mWifiButton->setFont(font);
    connect(mWifiButton, SIGNAL(clicked()), SLOT(onSwitchClicked()));

    mWifiLable = new QLabel(this);
    mWifiLable->setText("WIFI");
    mWifiLable->setStyleSheet("color: #b0b0b0;font-size:28px;");

    QLabel *connectedLable = new QLabel(this);
    connectedLable->setText("connected");
    connectedLable->setStyleSheet("color: #b0b0b0;font-size:24px;");

    mWifiName.clear();

    mConnectedWifi = new QLabel();

    memset(&status, 0x0, sizeof(wifi_status_t));
    wifimanager_get_status(&status);

    QString fileName = AIC_WIFI_CONFIG_FILE;
    mSetting = new QSettings(fileName, QSettings::IniFormat);

    int wifiEn = mSetting->value("wifimanager", 0).toInt();

    if (wifiEn) {
        mWifiButton->setText("Close");
        mWifiOpen = true;

        if ((status.state == WIFI_STATE_GOT_IP) ||
                               (status.state == WIFI_STATE_CONNECTED) ||
                               (status.state == WIFI_STATE_DHCPC_REQUEST)) {
            QString wifiSsid = QString::fromUtf8(status.ssid);
            mConnectedWifi->setText(wifiSsid);
            mConnectedWifi->setStyleSheet("color: #0c84ff;font-size:24px;");

            mWifiName = wifiSsid;
        } else {
            mConnectedWifi->setText("NULL");
            mConnectedWifi->setStyleSheet("min-height: 50px;min-width: 200px;background: #bbbbbb;font-size:28px;");
        }

        wifimanager_scan();

    } else {
        mWifiButton->setText("Open");
        mWifiOpen = false;

        mConnectedWifi->setText("NULL");
        mConnectedWifi->setStyleSheet("min-height: 50px;min-width: 200px;background: #bbbbbb;font-size:28px;");
    }

    mLayout->addWidget(mWifiLable, 0, 0);
    mLayout->addWidget(mWifiButton, 0, 1);
    mLayout->addWidget(connectedLable, 1, 0);
    mLayout->addWidget(mConnectedWifi, 1, 1);
    mLayout->addWidget(mScrollArea, 2, 0, 1, 2);

    mScrollArea->setWidgetResizable(true);

    mKeyboardWin = new QWidget;
    mKeyboardWin->resize(850, 370);
    mKeyboardWin->setWindowFlags(Qt::Window);

    mKeyBoard = new Keyboard;
    mKeyBoard->show();

    mTextInput = new QLineEdit(mKeyBoard);
    mTextInput->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    mTextInput->setStyleSheet(qss);

    mKeyBoard->mKeyboardWin = mKeyboardWin;
    mKeyBoard->mTextInput = mTextInput;

    QVBoxLayout *v = new QVBoxLayout;
    v->addWidget(mTextInput, 1);
    v->addWidget(mKeyBoard, 5);
    mKeyboardWin->setLayout(v);

    mWifiThread =  new AiCWifiThread();
    connect(mWifiThread, SIGNAL(updateUI(QString)), this, SLOT(onUpdateUI(QString)));
    connect(mWifiThread, SIGNAL(updateStatUI(int)), this, SLOT(onUpdateStatUI(int)));
    mWifiThread->start();
    mWifiListWin = NULL;
}

 QWidget *AiCConfigView::createWifiWidget(QWidget *parent, QString name)
{
    QWidget *widget = new QWidget(parent);
    QHBoxLayout *hLayout = new QHBoxLayout(widget);

    widget->setFixedSize(600, 80);
    widget->setObjectName(name);

    QLabel *wifi = new QLabel(widget);
    wifi->setText(name);
    wifi->setStyleSheet("color: #b0b0b0;font-size:28px;border-bottom");

    hLayout->addWidget(wifi);

    widget->installEventFilter(this);

    return widget;
}

bool AiCConfigView::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        mKeyBoard->mwifiName = obj->objectName();
        mWifiName = obj->objectName();
        mwifiWidget = qobject_cast<QWidget *>(obj);

        mTextInput->setText("");
        mKeyboardWin->show();
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease)
        return true;

    return QWidget::eventFilter(obj, event);
}

void AiCConfigView::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(this->rect(),QColor(255,255,255));

    QWidget::paintEvent(event);
}

void AiCConfigView::onSwitchClicked()
{
    mWifiOpen = !mWifiOpen;

    QString str = mWifiOpen ? "Close" : "Open";
    mWifiButton->setText(str);

    if (mWifiOpen) {
        mSetting->setValue("wifimanager", 1);
        mSetting->sync();

        wifimanager_scan();
    } else {
        mSetting->setValue("wifimanager", 0);
        mSetting->sync();

        if (!mWifiName.isEmpty()) {
            QByteArray ba = mWifiName.toLatin1();
            char *ssid = ba.data();

            /* remove wifi AP */
            wifimanager_remove_networks(ssid, mWifiName.size());
            mWifiName.clear();

            mConnectedWifi->setText("NULL");
            mConnectedWifi->setStyleSheet("min-height: 50px;min-width: 200px;background: #bbbbbb;font-size:28px;");
        }

        if (mWifiListWin)  {
            delete mWifiListWin;
            mWifiListWin = NULL;
        }
    }
}

void AiCConfigView::onUpdateStatUI(int stat)
{
    if (stat == WIFI_STATE_DISCONNECTED) {
        mConnectedWifi->setText("NULL");
        mConnectedWifi->setStyleSheet("min-height: 50px;min-width: 200px;background: #bbbbbb;font-size:28px;");
        mWifiName.clear();
    }

    if (stat == WIFI_STATE_ERROR) {
        mConnectedWifi->setText("ERROR");
        mConnectedWifi->setStyleSheet("color: #f00000;font-size:24px;");
        mWifiName.clear();
     }

    if ((stat == WIFI_STATE_GOT_IP) ||
        (stat == WIFI_STATE_CONNECTED) ||
        (stat == WIFI_STATE_DHCPC_REQUEST)) {
        mConnectedWifi->setText(mWifiName);
        mConnectedWifi->setStyleSheet("color: #0c84ff;font-size:24px;");
        mwifiWidget->hide();
    }
}

void AiCConfigView::onUpdateUI(QString content)
{
    QStringList wifiList;
    QStringList qStrLine = content.split("\n");

    foreach (QString line, qStrLine) {
        QStringList arr = line.split("\t");

        if (arr.size() < 4)
            continue;

        QString wifiName = arr.at(4);

        if (wifiName.isEmpty() || wifiName.compare(mWifiName) == 0)
            continue;

        if (!wifiList.contains(wifiName))
            wifiList.append(wifiName);
    }

    mWifiListWin = new QWidget(this);
    QVBoxLayout *vLayout = new QVBoxLayout(mWifiListWin);

    vLayout->setSpacing(20);
    mScrollArea->setWidget(mWifiListWin);

    foreach (QString wifi, wifiList) {
        QWidget *Widget = createWifiWidget(mWifiListWin, wifi);

        vLayout->addWidget(Widget, 0, Qt::AlignTop);
    }
}

AiCConfigView::~AiCConfigView()
{
    int ret;
    key_t key = ftok("/tmp", 777);
    int msgid = msgget(key, IPC_CREAT | 0666);

    struct wifiMsg msg;
    msg.mtype = MSG_TYPE;
    char *data = msg.mtext;

    data[0] = 'E';
    data[1] = 'X';
    data[2] = 'I';
    data[3] = 'T';

    ret = msgsnd(msgid, &msg, MAX_SIZE, 0);
    if (ret)
        qDebug("msgsnd Thread EXIT failed\n");

    if (mWifiListWin) {
        delete mWifiListWin;
        mWifiListWin = NULL;
    }
}
