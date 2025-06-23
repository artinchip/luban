/*
 * Copyright (C) 2024-2025 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 *         Huahui Mai <huahui.mai@artinchip.com>
 */

#ifndef AICTYPES_H
#define AICTYPES_H

enum WifiStatusType {
    WIFI_OFF = 0,
    WIFI_ON  = 1,
};

enum AiCFontSize {
    FONT_SMALL = 10,
    FONT_NORMAL = 12,
    FONT_BIG = 14,
    FONT_LARGE = 16,

};

#define AIC_WIFI_CONFIG_FILE "/tmp/qtlauncher.ini"

#endif // AICTYPES_H
