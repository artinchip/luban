#ifndef AICDESKTOPBUTTON_H
#define AICDESKTOPBUTTON_H

#include "utils/aictypes.h"

#include <QPushButton>
#include <QSize>
#include <QColor>
#include <QBrush>
#include <QPixmap>
#include <QString>
#include <QHBoxLayout>
#include <QLabel>

class AiCDesktopButton : public QPushButton
{
public:
enum AiCDesktopColor {
        BG_WHITE,
        BG_DEEP_RED,
        BG_LIGHT_RED,
        BG_DEEP_GREEN,
        BG_LIGHT_GREEN,
        BG_DEEP_PURPLE,
        BG_LIGHT_PURPLE,
        BG_DEEP_BLUE,
        BG_LIGHT_BLUE,
        BG_DEEP_YELLOW,
        BG_LIGHT_YELLOW,
};

public:
    AiCDesktopButton();
    AiCDesktopButton(QSize size, AiCDesktopColor color);
    ~AiCDesktopButton();

    void setParameters(QPixmap pixmap, QString title);
    void adjustSize();

protected:
    void paintEvent(QPaintEvent *event);

private:
    void initButton();
    QColor getColor(AiCDesktopColor color);


private:
   AiCDesktopColor mBGColor;
   QLabel *mIconLabel;
   QLabel *mTitleLabel;
   QSize mSize;
   QHBoxLayout *mLayout;
};

#endif // AICDESKTOPBUTTON_H
