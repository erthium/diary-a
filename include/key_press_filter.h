#ifndef KEY_PRESS_FILTER_H
#define KEY_PRESS_FILTER_H

#include <QObject>
#include <QPushButton>
#include <QEvent>
#include <QKeyEvent>

class KeyPressFilter : public QObject {
public:
    KeyPressFilter(QPushButton *toggle_button, QPushButton *enter_button);
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QPushButton *toggle_button;
    QPushButton *enter_button;
};

#endif // KEY_PRESS_FILTER_H
