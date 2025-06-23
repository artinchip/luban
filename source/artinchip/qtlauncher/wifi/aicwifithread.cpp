/*
 * Copyright (C) 2025, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors: Huahui Mai <huahui.mai@artinchip.com>
 */

#include "aicwifithread.h"

#include <QDebug>
#include <QString>

#include <wifimanager.h>

AiCWifiThread::AiCWifiThread()
{

}

static const char *disconn_reason2string(wifimanager_disconn_reason_t reason)
{
    switch (reason) {
    case AUTO_DISCONNECT:
        return "wpa auto disconnect";
    case ACTIVE_DISCONNECT:
        return "active disconnect";
    case KEYMT_NO_SUPPORT:
        return "keymt is not supported";
    case CMD_OR_PARAMS_ERROR:
        return "wpas command error";
    case IS_CONNECTTING:
        return "wifi is still connecting";
    case CONNECT_TIMEOUT:
        return "connect timeout";
    case REQUEST_IP_TIMEOUT:
        return "request ip address timeout";
    case WPA_TERMINATING:
        return "wpa_supplicant is closed";
    case AP_ASSOC_REJECT:
        return "AP assoc reject";
    case NETWORK_NOT_FOUND:
        return "can't search such ssid";
    case PASSWORD_INCORRECT:
        return "incorrect password";
    default:
        return "other reason";
    }
}

void AiCWifiThread::run(void)
{
    struct wifiMsg msg;
    int ret;

    key_t key = ftok("/tmp", 777);
    int msgid = msgget(key, IPC_CREAT | 0666);

    while (1) {
        ret = msgrcv(msgid, &msg, MAX_SIZE, MSG_TYPE, 0);
        if (ret == -1) {
            qDebug() << "msgcrv error " << errno << strerror(errno);
            continue;
        }

        char *data = msg.mtext;

        if (data[0] == 'E' && data[1] == 'X' && data[2] == 'I' && data[3] == 'T')
            break;

        if (data[0] == 0xEE) {
            int stat = (int)data[1];
            int reason = (int)data[2];

            if (stat == WIFI_STATE_DISCONNECTED)
                qDebug("disconnect reason: %s\n",
                       disconn_reason2string((wifimanager_disconn_reason_t)reason));

            emit updateStatUI(stat);
            continue;
        }

        QString str(msg.mtext);
        emit updateUI(str);
    }

    emit finished();
}

AiCWifiThread::~AiCWifiThread()
{

}
