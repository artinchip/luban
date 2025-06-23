/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#include "aicanimastateview.h"

class Pixmap : public QGraphicsObject
{
public:
    Pixmap(const QPixmap &pix) : QGraphicsObject(), p(pix)
    {
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
    {
        painter->drawPixmap(QPointF(), p);
    }

    QRectF boundingRect() const
    {
        return QRectF( QPointF(0, 0), p.size());
    }

private:
    QPixmap p;
};


AiCAnimaStateView::AiCAnimaStateView(QSize size, QWidget *parent) : QWidget(parent)
{
    initView(size.width(), size.height());
}

void AiCAnimaStateView::initView(int width, int height)
{
    mGraphicsScene = new QGraphicsScene(0, 0, 500, 400);
    mGraphicsView = new QGraphicsView(mGraphicsScene, this);
    mGraphicsView->setFixedSize(width,height);
    mGraphicsView->setAlignment(Qt::AlignCenter);
    //mGraphicsScene->setBackgroundBrush(mGraphicsScene->palette().window());

    // Text edit and button
    QTextEdit *edit = new QTextEdit;
    edit->setText("ArtInChip is an IC design company focusing on industry, "
                  "1602 is a RISC-V SoC with 128MB ddr3 sipped, "
                  "Luban is a full feature SDK with UI and Player!");

    mButton = new QPushButton();
    QGraphicsProxyWidget *buttonProxy = new QGraphicsProxyWidget;
    buttonProxy->setWidget(mButton);
    QGraphicsProxyWidget *editProxy = new QGraphicsProxyWidget;
    editProxy->setWidget(edit);

    QGroupBox *box = new QGroupBox;
    box->setFlat(true);
    box->setTitle("Options");

    QVBoxLayout *layout2 = new QVBoxLayout;
    box->setLayout(layout2);
    layout2->addWidget(new QRadioButton("RadioButton 1"));
    layout2->addWidget(new QRadioButton("RadioButton 2"));
    layout2->addWidget(new QRadioButton("RadioButton 3"));
    layout2->addStretch();

    QGraphicsProxyWidget *boxProxy = new QGraphicsProxyWidget;
    boxProxy->setWidget(box);

    // Parent widget
    QGraphicsWidget *widget = new QGraphicsWidget;
    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(Qt::Vertical, widget);
    layout->addItem(editProxy);
    layout->addItem(buttonProxy);
    widget->setLayout(layout);

    Pixmap *p1 = new Pixmap(QPixmap(":/resources/animation/icon1.png"));
    Pixmap *p2 = new Pixmap(QPixmap(":/resources/animation/icon2.png"));
    Pixmap *p3 = new Pixmap(QPixmap(":/resources/animation/icon3.png"));
    Pixmap *p4 = new Pixmap(QPixmap(":/resources/animation/icon4.png"));
    Pixmap *p5 = new Pixmap(QPixmap(":/resources/animation/icon5.png"));
    Pixmap *p6 = new Pixmap(QPixmap(":/resources/animation/icon6.png"));

    mGraphicsScene->addItem(widget);
    mGraphicsScene->addItem(boxProxy);
    mGraphicsScene->addItem(p1);
    mGraphicsScene->addItem(p2);
    mGraphicsScene->addItem(p3);
    mGraphicsScene->addItem(p4);
    mGraphicsScene->addItem(p5);
    mGraphicsScene->addItem(p6);

    mMachine = new QStateMachine();
    QState *state1 = new QState(mMachine);
    QState *state2 = new QState(mMachine);
    QState *state3 = new QState(mMachine);
    mMachine->setInitialState(state1);

    // State 1
    state1->assignProperty(mButton, "text", "Click to switch 2");
    state1->assignProperty(widget, "geometry", QRectF(0, 0, 400, 150));
    state1->assignProperty(box, "geometry", QRect(-200, 150, 200, 150));
    state1->assignProperty(p1, "pos", QPointF(68, 200)); // 185));
    state1->assignProperty(p2, "pos", QPointF(168, 200)); // 185));
    state1->assignProperty(p3, "pos", QPointF(268, 200)); // 185));
    state1->assignProperty(p4, "pos", QPointF(68 - 150, 48 - 150));
    state1->assignProperty(p5, "pos", QPointF(168, 48 - 150));
    state1->assignProperty(p6, "pos", QPointF(268 + 150, 48 - 150));
    state1->assignProperty(p1, "rotation", qreal(0));
    state1->assignProperty(p2, "rotation", qreal(0));
    state1->assignProperty(p3, "rotation", qreal(0));
    state1->assignProperty(p4, "rotation", qreal(-270));
    state1->assignProperty(p5, "rotation", qreal(-90));
    state1->assignProperty(p6, "rotation", qreal(270));
    state1->assignProperty(boxProxy, "opacity", qreal(0));
    state1->assignProperty(p1, "opacity", qreal(1));
    state1->assignProperty(p2, "opacity", qreal(1));
    state1->assignProperty(p3, "opacity", qreal(1));
    state1->assignProperty(p4, "opacity", qreal(0));
    state1->assignProperty(p5, "opacity", qreal(0));
    state1->assignProperty(p6, "opacity", qreal(0));

    // State 2
    state2->assignProperty(mButton, "text", "Click to switch 3");
    state2->assignProperty(widget, "geometry", QRectF(200, 150, 200, 150));
    state2->assignProperty(box, "geometry", QRect(9, 150, 190, 150));
    state2->assignProperty(p1, "pos", QPointF(68 - 150, 185 + 150));
    state2->assignProperty(p2, "pos", QPointF(168, 185 + 150));
    state2->assignProperty(p3, "pos", QPointF(268 + 150, 185 + 150));
    state2->assignProperty(p4, "pos", QPointF(64, 48));
    state2->assignProperty(p5, "pos", QPointF(168, 48));
    state2->assignProperty(p6, "pos", QPointF(268, 48));
    state2->assignProperty(p1, "rotation", qreal(-270));
    state2->assignProperty(p2, "rotation", qreal(90));
    state2->assignProperty(p3, "rotation", qreal(270));
    state2->assignProperty(p4, "rotation", qreal(0));
    state2->assignProperty(p5, "rotation", qreal(0));
    state2->assignProperty(p6, "rotation", qreal(0));
    state2->assignProperty(boxProxy, "opacity", qreal(1));
    state2->assignProperty(p1, "opacity", qreal(0));
    state2->assignProperty(p2, "opacity", qreal(0));
    state2->assignProperty(p3, "opacity", qreal(0));
    state2->assignProperty(p4, "opacity", qreal(1));
    state2->assignProperty(p5, "opacity", qreal(1));
    state2->assignProperty(p6, "opacity", qreal(1));

    // State 3
    state3->assignProperty(mButton, "text", "Click to switch 1");
    state3->assignProperty(p1, "pos", QPointF(0, 5));
    state3->assignProperty(p2, "pos", QPointF(0, 5 + 64 + 5));
    state3->assignProperty(p3, "pos", QPointF(5, 5 + (64 + 5) + 64));
    state3->assignProperty(p4, "pos", QPointF(5 + 64 + 5, 5));
    state3->assignProperty(p5, "pos", QPointF(5 + 64 + 5, 5 + 64 + 5));
    state3->assignProperty(p6, "pos", QPointF(5 + 64 + 5, 5 + (64 + 5) + 64));
    state3->assignProperty(widget, "geometry", QRectF(138, 5, 400 - 138, 200));
    state3->assignProperty(box, "geometry", QRect(5, 205, 400, 90));
    state3->assignProperty(p1, "opacity", qreal(1));
    state3->assignProperty(p2, "opacity", qreal(1));
    state3->assignProperty(p3, "opacity", qreal(1));
    state3->assignProperty(p4, "opacity", qreal(1));
    state3->assignProperty(p5, "opacity", qreal(1));
    state3->assignProperty(p6, "opacity", qreal(1));

    QAbstractTransition *t1 = state1->addTransition(mButton, SIGNAL(clicked()), state2);
    QSequentialAnimationGroup *animation1SubGroup = new QSequentialAnimationGroup;
    animation1SubGroup->addPause(250);
    animation1SubGroup->addAnimation(new QPropertyAnimation(box, "geometry"));
    t1->addAnimation(animation1SubGroup);
    t1->addAnimation(new QPropertyAnimation(widget, "geometry"));
    t1->addAnimation(new QPropertyAnimation(p1, "pos"));
    t1->addAnimation(new QPropertyAnimation(p2, "pos"));
    t1->addAnimation(new QPropertyAnimation(p3, "pos"));
    t1->addAnimation(new QPropertyAnimation(p4, "pos"));
    t1->addAnimation(new QPropertyAnimation(p5, "pos"));
    t1->addAnimation(new QPropertyAnimation(p6, "pos"));
    t1->addAnimation(new QPropertyAnimation(p1, "rotation"));
    t1->addAnimation(new QPropertyAnimation(p2, "rotation"));
    t1->addAnimation(new QPropertyAnimation(p3, "rotation"));
    t1->addAnimation(new QPropertyAnimation(p4, "rotation"));
    t1->addAnimation(new QPropertyAnimation(p5, "rotation"));
    t1->addAnimation(new QPropertyAnimation(p6, "rotation"));
    t1->addAnimation(new QPropertyAnimation(p1, "opacity"));
    t1->addAnimation(new QPropertyAnimation(p2, "opacity"));
    t1->addAnimation(new QPropertyAnimation(p3, "opacity"));
    t1->addAnimation(new QPropertyAnimation(p4, "opacity"));
    t1->addAnimation(new QPropertyAnimation(p5, "opacity"));
    t1->addAnimation(new QPropertyAnimation(p6, "opacity"));

    QAbstractTransition *t2 = state2->addTransition(mButton, SIGNAL(clicked()), state3);
    t2->addAnimation(new QPropertyAnimation(box, "geometry"));
    t2->addAnimation(new QPropertyAnimation(widget, "geometry"));
    t2->addAnimation(new QPropertyAnimation(p1, "pos"));
    t2->addAnimation(new QPropertyAnimation(p2, "pos"));
    t2->addAnimation(new QPropertyAnimation(p3, "pos"));
    t2->addAnimation(new QPropertyAnimation(p4, "pos"));
    t2->addAnimation(new QPropertyAnimation(p5, "pos"));
    t2->addAnimation(new QPropertyAnimation(p6, "pos"));
    t2->addAnimation(new QPropertyAnimation(p1, "rotation"));
    t2->addAnimation(new QPropertyAnimation(p2, "rotation"));
    t2->addAnimation(new QPropertyAnimation(p3, "rotation"));
    t2->addAnimation(new QPropertyAnimation(p4, "rotation"));
    t2->addAnimation(new QPropertyAnimation(p5, "rotation"));
    t2->addAnimation(new QPropertyAnimation(p6, "rotation"));
    t2->addAnimation(new QPropertyAnimation(p1, "opacity"));
    t2->addAnimation(new QPropertyAnimation(p2, "opacity"));
    t2->addAnimation(new QPropertyAnimation(p3, "opacity"));
    t2->addAnimation(new QPropertyAnimation(p4, "opacity"));
    t2->addAnimation(new QPropertyAnimation(p5, "opacity"));
    t2->addAnimation(new QPropertyAnimation(p6, "opacity"));

    QAbstractTransition *t3 = state3->addTransition(mButton, SIGNAL(clicked()), state1);
    t3->addAnimation(new QPropertyAnimation(box, "geometry"));
    t3->addAnimation(new QPropertyAnimation(widget, "geometry"));
    t3->addAnimation(new QPropertyAnimation(p1, "pos"));
    t3->addAnimation(new QPropertyAnimation(p2, "pos"));
    t3->addAnimation(new QPropertyAnimation(p3, "pos"));
    t3->addAnimation(new QPropertyAnimation(p4, "pos"));
    t3->addAnimation(new QPropertyAnimation(p5, "pos"));
    t3->addAnimation(new QPropertyAnimation(p6, "pos"));
    t3->addAnimation(new QPropertyAnimation(p1, "rotation"));
    t3->addAnimation(new QPropertyAnimation(p2, "rotation"));
    t3->addAnimation(new QPropertyAnimation(p3, "rotation"));
    t3->addAnimation(new QPropertyAnimation(p4, "rotation"));
    t3->addAnimation(new QPropertyAnimation(p5, "rotation"));
    t3->addAnimation(new QPropertyAnimation(p6, "rotation"));
    t3->addAnimation(new QPropertyAnimation(p1, "opacity"));
    t3->addAnimation(new QPropertyAnimation(p2, "opacity"));
    t3->addAnimation(new QPropertyAnimation(p3, "opacity"));
    t3->addAnimation(new QPropertyAnimation(p4, "opacity"));
    t3->addAnimation(new QPropertyAnimation(p5, "opacity"));
    t3->addAnimation(new QPropertyAnimation(p6, "opacity"));

    mMachine->start();
}

void AiCAnimaStateView::initStateMachine()
{

}

AiCAnimaStateView::~AiCAnimaStateView()
{
    delete mGraphicsView;
    delete mGraphicsScene;
}
