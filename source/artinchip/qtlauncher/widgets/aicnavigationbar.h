#ifndef AICNAVIGATIONBAR_H
#define AICNAVIGATIONBAR_H

#include <QWidget>
#include <QHBoxLayout>
#include <QPaintEvent>
#include <QPushButton>
#include <QPixmap>
#include <QSize>

class AiCNavigationBar : public QWidget
{
    Q_OBJECT
public:
    explicit AiCNavigationBar(QWidget *parent = 0);
    explicit AiCNavigationBar(QSize size, QWidget *parent = 0);
    ~AiCNavigationBar();

protected:
    void paintEvent(QPaintEvent *event);
    void initBar();

signals:
    void MenuClick();
    void HomeClick();
    void BackClick();

public slots:

private:
    QSize mSize;
    QHBoxLayout *mLayout;
    QPushButton *mMenuButton;
    QPushButton *mHomeButton;
    QPushButton *mBackButton;
    QPixmap mBGPixmap;
};

#endif // AICNAVIGATIONBAR_H
