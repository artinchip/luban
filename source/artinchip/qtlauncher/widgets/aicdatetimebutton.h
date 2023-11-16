#ifndef AICDATETIMEBUTTON_H
#define AICDATETIMEBUTTON_H

#include "widgets/aictimewidget.h"

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QPaintEvent>
#include <QVBoxLayout>
#include <QLCDNumber>

class AiCDateTimeButton : public QPushButton
{
    Q_OBJECT
public:
    explicit AiCDateTimeButton(QWidget *parent = 0);
    explicit AiCDateTimeButton(QSize size, QWidget *parent = 0);
     ~AiCDateTimeButton();

protected:
    void paintEvent(QPaintEvent *event);

private:
    void initButton();
signals:

public slots:

protected slots:
    void onCountTimeOut();

private:
   QLabel *mDateLabel;
   QLabel *mTimeLabel;
   QLCDNumber *mTimeNum;
   AiCTimeWidget *mTimeWidget;
   QVBoxLayout *mLayout;

   QSize mSize;
   QTimer *mTimer;
};

#endif // AICDATETIMEBUTTON_H
