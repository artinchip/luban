/*
 * Copyright (C) 2025 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AICCONFIGVIEW_H
#define AICCONFIGVIEW_H

#include "keyboard/Keyboard.h"
#include "wifi/aicwifithread.h"

#include <QWidget>
#include <QLabel>
#include <QDebug>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QGridLayout>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSettings>

#include <wifimanager.h>

using AeaQt::Keyboard;

class AiCConfigView : public QWidget
{
    Q_OBJECT

public:
    AiCConfigView(QSize size, QWidget *parent = NULL);
    ~AiCConfigView();

private:
    void initView(int width, int height);
    QWidget *createWifiWidget(QWidget *parent, QString name);

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual bool eventFilter(QObject *obj, QEvent *event);

public slots:
    void onSwitchClicked();
    void onUpdateUI(QString content);
    void onUpdateStatUI(int stat);

private:
    QWidget *mWifiListWin;
    QWidget *mwifiWidget;
    QWidget *mKeyboardWin;
    QPushButton *mWifiButton;
    QLabel *mWifiLable;
    QLabel *mConnectedWifi;
    QGridLayout *mLayout;
    QScrollArea *mScrollArea;
    QLineEdit *mTextInput;
    QString mWifiName;
    QSettings *mSetting;

private:
    bool mWifiOpen;
    Keyboard *mKeyBoard;
    AiCWifiThread *mWifiThread;
    wifimanager_cb_t mwifimanager_cb_t;
};

#endif // AICCONFIGVIEW_H
