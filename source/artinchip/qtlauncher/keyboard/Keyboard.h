/**********************************************************
#Author: Qtjun
#WeChat Official Accounts: qthub_com
#QQ Group: 732271126
#Email: 2088201923@qq.com
**********************************************************/

#ifndef AEA_KEYBOARD_H
#define AEA_KEYBOARD_H

#include "AbstractKeyboard.h"
#include "KeyButton.h"
#include <QLayout>
#include <QListWidget>
#include <QHash>
#include <QPair>
#include <QString>

#define KEYBOARD_CHINESE_SUPPORT 0

namespace AeaQt {

#if KEYBOARD_CHINESE_SUPPORT
class ChineseWidget : public QListWidget {
    Q_OBJECT
public:
    ChineseWidget(QWidget *parent = NULL);
    void setText(const QString &text);

signals:
    void pressedChanged(const int &code, const QString &text);

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    void addOneItem(const QString &text);
    void loadChineseLib();
    void loadChinesePhraseLib();
    void loadGoogleChineseLib();

private:
    QMap<QString, QList<QPair<QString, QString> > > m_data;
};
#endif

class Keyboard : public AbstractKeyboard
{
    Q_OBJECT
public:
    Keyboard(QWidget *parent = NULL);

protected:
    void resizeEvent(QResizeEvent *e);

private slots:
    void switchCapsLock();
    void switchSpecialChar();
    void switchEnOrCh();
    void onButtonPressed(const int &code, const QString &text);
    void onEnterPressed();
#if KEYBOARD_CHINESE_SUPPORT
    void clearBufferText();
#endif

public:
    QString mwifiName;
    QWidget *mKeyboardWin;
    QLineEdit *mTextInput;

private:
    KeyButton *createButton(QList<KeyButton::Mode> modes);

    QWidget *createBar(const QList<QList<KeyButton::Mode> > &modes);
    QHBoxLayout *h1();
    QHBoxLayout *h2();
    QHBoxLayout *h3();
    QHBoxLayout *h4();
#if KEYBOARD_CHINESE_SUPPORT
    QWidget *candidateList();
    QWidget *chineseBar();
#endif

    void resizeButton();

private:
    bool m_isChinese;
#if KEYBOARD_CHINESE_SUPPORT
    ChineseWidget *m_chineseWidget;
    QString m_bufferText;
#endif
};

}
#endif // AEA_KEYBOARD_H
