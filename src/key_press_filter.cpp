#include <key_press_filter.h>

KeyPressFilter::KeyPressFilter(QPushButton *toggle_button, QPushButton *enter_button)
    : toggle_button(toggle_button), enter_button(enter_button) {}

bool KeyPressFilter::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            static_cast<QWidget *>(obj)->close();  // Close the window when ESC is pressed
            return true;  // Event handled
        } else if (keyEvent->key() == Qt::Key_Alt) {
            toggle_button->click();
            return true;  // Event handled
        } else if (keyEvent->key() == Qt::Key_Return) {
            enter_button->click();
            return true;  // Event handled
        }
    }
    return QObject::eventFilter(obj, event);  // Default event processing
}
