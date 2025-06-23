/*
 * Copyright (C) 2025, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors: Huahui Mai <huahui.mai@artinchip.com>
 */

#ifndef AICWIFITHREAD_H
#define AICWIFITHREAD_H

#include <QThread>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MAX_SIZE	4096
#define MSG_TYPE	1

struct wifiMsg
{
    long mtype;
    char mtext[MAX_SIZE];
};

class AiCWifiThread : public QThread
{
    Q_OBJECT
public:
    AiCWifiThread();
    ~AiCWifiThread();

public:
    void run();

signals:
    void updateUI(QString content);
    void updateStatUI(int stat);
};

#endif // AICWIFITHREAD_H
