/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#include "aicanimatilesview.h"

#include <QGraphicsScene>
#include <QPixmap>
#include <QtGui>

#include <QtCore/qstate.h>
#include "widgets/aicgraphicsbutton.h"

AiCAnimaTilesView::AiCAnimaTilesView(QSize size, QWidget *parent) : QWidget(parent)
{
    initView(size.width(), size.height());
}

void AiCAnimaTilesView::initView(int width, int height)
{
    int buttonX,buttonY;
    QPixmap kineticPix(":/resources/tiles/kinetic.png");
    QPixmap bgPix(":/resources/tiles/Time-For-Lunch-2.jpg");

    mGraphicsScene = new QGraphicsScene(0, 0, width - 20, height - 20);
    mGraphicsView = new QGraphicsView(mGraphicsScene, this);
    mGraphicsView->setFixedSize(width, height);
    mGraphicsView->setAlignment(Qt::AlignCenter);
    mGraphicsView->setBackgroundBrush(bgPix);
    mGraphicsView->setCacheMode(QGraphicsView::CacheBackground);
    //mGraphicsView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    QList<AiCGraphicsButtonPixmap *> items;
    for (int i = 0; i < 64; ++i) {
        AiCGraphicsButtonPixmap *item = new AiCGraphicsButtonPixmap(kineticPix);
        item->setOffset(width/2 - kineticPix.width()/2, height/2 - kineticPix.height()/2);
        item->setZValue(i);
        items << item;
        mGraphicsScene->addItem(item);
    }

    // Buttons
    QGraphicsItem *buttonParent = new QGraphicsRectItem;
    AiCGraphicsButton *ellipseButton = new AiCGraphicsButton(QPixmap(":/resources/tiles/icon1.png"), buttonParent);
    AiCGraphicsButton *figure8Button = new AiCGraphicsButton(QPixmap(":/resources/tiles/icon2.png"), buttonParent);
    AiCGraphicsButton *randomButton = new AiCGraphicsButton(QPixmap(":/resources/tiles/icon3.png"), buttonParent);
    AiCGraphicsButton *tiledButton = new AiCGraphicsButton(QPixmap(":/resources/tiles/icon4.png"), buttonParent);
    AiCGraphicsButton *centeredButton = new AiCGraphicsButton(QPixmap(":/resources/tiles/icon5.png"), buttonParent);

    buttonX = width - 200;
    buttonY = height - 200;
    ellipseButton->setPos(-100, -100);
    figure8Button->setPos(100, -100);
    randomButton->setPos(0, 0);
    tiledButton->setPos(-100, 100);
    centeredButton->setPos(100, 100);

    mGraphicsScene->addItem(buttonParent);
    buttonParent->scale(0.75, 0.75);
    buttonParent->setPos(buttonX, buttonY);
    buttonParent->setZValue(65);

    // States
    QState *rootState = new QState;
    QState *ellipseState = new QState(rootState);
    QState *figure8State = new QState(rootState);
    QState *randomState = new QState(rootState);
    QState *tiledState = new QState(rootState);
    QState *centeredState = new QState(rootState);

    // Values
    for (int i = 0; i < items.count(); ++i) {
        AiCGraphicsButtonPixmap *item = items.at(i);
        // Ellipse
        ellipseState->assignProperty(item, "pos",
                                    QPointF(cos((i / 63.0) * 6.28) * 400,
                                    sin((i / 63.0) * 6.28) * 200));

        // Figure 8
        figure8State->assignProperty(item, "pos",
                                     QPointF(sin((i / 63.0) * 6.28) * 400,
                                     sin(((i * 2)/63.0) * 6.28) * 200));

        // Random
        randomState->assignProperty(item, "pos",
                                    QPointF(-250 + qrand() % 400,
                                    -250 + qrand() % 400));

         // Tiled
         tiledState->assignProperty(item, "pos",
                                    QPointF(((i % 8) - 4) * kineticPix.width() + kineticPix.width() / 2,
                                    ((i / 8) - 4) * kineticPix.height() + kineticPix.height() / 2));

         // Centered
         centeredState->assignProperty(item, "pos", QPointF());
    }

    mMachine = new QStateMachine();
    mMachine->addState(rootState);
    mMachine->setInitialState(rootState);
    rootState->setInitialState(tiledState);

    QParallelAnimationGroup *group = new QParallelAnimationGroup;
    for (int i = 0; i < items.count(); ++i) {
        QPropertyAnimation *anim = new QPropertyAnimation(items[i], "pos");
        anim->setDuration(750 + i * 25);
        anim->setEasingCurve(QEasingCurve::InOutBack);
        group->addAnimation(anim);
    }
    QAbstractTransition *trans = rootState->addTransition(ellipseButton, SIGNAL(pressed()), ellipseState);
    trans->addAnimation(group);

    trans = rootState->addTransition(figure8Button, SIGNAL(pressed()), figure8State);
    trans->addAnimation(group);

    trans = rootState->addTransition(randomButton, SIGNAL(pressed()), randomState);
    trans->addAnimation(group);

    trans = rootState->addTransition(tiledButton, SIGNAL(pressed()), tiledState);
    trans->addAnimation(group);

    trans = rootState->addTransition(centeredButton, SIGNAL(pressed()), centeredState);
    trans->addAnimation(group);

    trans->addAnimation(group);

    mMachine->start();
}

AiCAnimaTilesView::~AiCAnimaTilesView()
{
    delete mGraphicsView;
    delete mGraphicsScene;
}
