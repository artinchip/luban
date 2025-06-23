/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#ifndef AICANIMASTATEVIEW_H
#define AICANIMASTATEVIEW_H

#include <QWidget>
#include <QtGui>

class AiCAnimaStateView : public QWidget
{
    Q_OBJECT
public:
    explicit AiCAnimaStateView(QSize size, QWidget *parent = 0);
    ~AiCAnimaStateView();

private:
    void initView(int width, int height);
    void initStateMachine();

signals:

public slots:

private:
    QPushButton *mButton;
    QGraphicsScene *mGraphicsScene;
    QGraphicsView *mGraphicsView;
    QStateMachine *mMachine;
};

#endif // AICANIMASTATEVIEW_H
