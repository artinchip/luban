#include "aicgraphicsbutton.h"

#include <QtGui>

AiCGraphicsButton::AiCGraphicsButton(const QPixmap &pixmap, QGraphicsItem *parent) :  QGraphicsWidget(parent), mPix(pixmap)
{
    setAcceptHoverEvents(true);
    setCacheMode(DeviceCoordinateCache);
}

void AiCGraphicsButton::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    bool down = option->state & QStyle::State_Sunken;
    QRectF r = boundingRect();
    QLinearGradient grad(r.topLeft(), r.bottomRight());
    grad.setColorAt(down ? 1 : 0, option->state & QStyle::State_MouseOver ? Qt::white : Qt::lightGray);
    grad.setColorAt(down ? 0 : 1, Qt::darkGray);
    painter->setPen(Qt::darkGray);
    painter->setBrush(grad);
    painter->drawEllipse(r);
    QLinearGradient grad2(r.topLeft(), r.bottomRight());
    grad.setColorAt(down ? 1 : 0, Qt::darkGray);
    grad.setColorAt(down ? 0 : 1, Qt::lightGray);
    painter->setPen(Qt::NoPen);
    painter->setBrush(grad);
    if (down)
        painter->translate(2, 2);
    painter->drawEllipse(r.adjusted(5, 5, -5, -5));
    painter->drawPixmap(-mPix.width()/2, -mPix.height()/2, mPix);
}

void AiCGraphicsButton::mousePressEvent(QGraphicsSceneMouseEvent *)
{
    emit pressed();
    update();
}
void AiCGraphicsButton::mouseReleaseEvent(QGraphicsSceneMouseEvent *)
{
    update();
}
