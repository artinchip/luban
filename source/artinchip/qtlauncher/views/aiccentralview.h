/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#ifndef AICCENTRALVIEW_H
#define AICCENTRALVIEW_H

#include <QWidget>
#include <QGridLayout>

class AiCCentralView : public QWidget
{
    Q_OBJECT
public:
    explicit AiCCentralView(QWidget *parent = 0);
    ~AiCCentralView();
    void addButtonWidget(QWidget *, int row, int column, int rowSpan, int columnSpan);


private:
    void initView();

signals:

public slots:

private:
    QGridLayout *mGridLayout;
};

#endif // AICCENTRALVIEW_H
