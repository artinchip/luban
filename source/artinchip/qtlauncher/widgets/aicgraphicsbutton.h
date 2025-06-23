/*
 * Copyright (C) 2024 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Author: artinchip
 */
#ifndef AICGRAPHICSBUTTON_H
#define AICGRAPHICSBUTTON_H

#include <QGraphicsWidget>

class AiCGraphicsButtonPixmap : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
    Q_PROPERTY(QPointF pos READ pos WRITE setPos)
public:
    AiCGraphicsButtonPixmap(const QPixmap &pix)
        : QObject(), QGraphicsPixmapItem(pix)
    {
        setCacheMode(DeviceCoordinateCache);
    }
};

class AiCGraphicsButton : public QGraphicsWidget
{
    Q_OBJECT
public:
    explicit AiCGraphicsButton(const QPixmap &pixmap, QGraphicsItem *parent = 0);

signals:
void pressed();

protected:
void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *);
void mousePressEvent(QGraphicsSceneMouseEvent *event);
void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

QRectF boundingRect() const
{
    return QRectF(-65, -65, 130, 130);
}

QPainterPath shape() const
{
    QPainterPath path;
    path.addEllipse(boundingRect());
    return path;
}

private:
    QPixmap mPix;

};

#endif // AICGRAPHICSBUTTON_H
