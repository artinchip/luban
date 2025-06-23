/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#ifndef AICANIMATILESVIEW_H
#define AICANIMATILESVIEW_H

#include <QWidget>
#include <QSize>
#include <QPushButton>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QStateMachine>

class AiCAnimaTilesView : public QWidget
{
    Q_OBJECT
public:
    explicit AiCAnimaTilesView(QSize size, QWidget *parent = 0);
    ~AiCAnimaTilesView();

private:
    void initView(int width, int height);

signals:

public slots:

private:
    QPushButton *mButton;
    QGraphicsScene *mGraphicsScene;
    QGraphicsView *mGraphicsView;
    QStateMachine *mMachine;
};

#endif // AICANIMATILESVIEW_H
