#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>
#include <QScrollArea>
#include <QFont>
#include <QFile>

#include <key_press_filter.h>
#include <entry.h>
#include <crypto.h>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle("Diary-A");
    window.setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
    QVBoxLayout *layout = new QVBoxLayout(&window);

    // Create button to switch modes
    QPushButton *toggleButton = new QPushButton("Switch to Reading Mode (Alt)");
    layout->addWidget(toggleButton);
    QStackedWidget *stackedWidget = new QStackedWidget;
    layout->addWidget(stackedWidget);

    // Writing mode (QTextEdit)
    QWidget *writeMode = new QWidget;
    QVBoxLayout *writeLayout = new QVBoxLayout(writeMode);

    QTextEdit *textEdit = new QTextEdit;
    textEdit->setPlaceholderText("Write your entry here...");
    writeLayout->addWidget(textEdit);

    QPushButton *saveButton = new QPushButton("Save Entry");
    saveButton->setDisabled(true);
    QObject::connect(textEdit, &QTextEdit::textChanged, [&]() {
        saveButton->setDisabled(textEdit->toPlainText().isEmpty());
    });
    QObject::connect(saveButton, &QPushButton::clicked, [&]() {
        logEntry(textEdit->toPlainText().toStdString());
        textEdit->clear();
        saveButton->setDisabled(true);
    });

    writeLayout->addWidget(saveButton);
    stackedWidget->addWidget(writeMode);

    // Reading mode (QLabel)
    QWidget *readMode = new QWidget;
    QVBoxLayout *readLayout = new QVBoxLayout(readMode);
    stackedWidget->addWidget(readMode);

    QScrollArea *scrollArea = new QScrollArea(readMode);
    QWidget *entryContainer = new QWidget(scrollArea);
    QVBoxLayout *entryContainerLayout = new QVBoxLayout(entryContainer);
    entryContainer->setLayout(entryContainerLayout);

    scrollArea->setWidget(entryContainer);
    scrollArea->setWidgetResizable(true);
    readLayout->addWidget(scrollArea);

    // Handle button click to toggle mode
    QObject::connect(toggleButton, &QPushButton::clicked, [&]() {
        if (stackedWidget->currentIndex() == 0) {
            // Switch to reading mode

            // Clear previous entries
            for (int i = entryContainerLayout->count() - 1; i >= 0; i--) {
                QLayoutItem *item = entryContainerLayout->takeAt(i);
                if (QWidget *widget = item->widget()) {
                    widget->deleteLater();
                }
                delete item;
            }

            // Add new entries
            for (const Entry &entry : entries) {
                QWidget *entryWidget = new QWidget;
                entryWidget->setObjectName("entryWidget");
                QVBoxLayout *entryLayout = new QVBoxLayout(entryWidget);
                QLabel *dateLabel = new QLabel(QString::fromStdString(entry.date));
                dateLabel->setObjectName("dateLabel");
                entryLayout->addWidget(dateLabel);
                QLabel *entryLabel = new QLabel(parseMarkdown(QString::fromStdString(entry.content)));
                entryLabel->setObjectName("entryLabel");
                entryLabel->setWordWrap(true);
                entryLayout->addWidget(entryLabel);
                entryContainerLayout->addWidget(entryWidget);
            }

            stackedWidget->setCurrentIndex(1);
            toggleButton->setText("Switch to Writing Mode (Alt)");
        } else {
            // Switch to writing mode
            stackedWidget->setCurrentIndex(0);
            toggleButton->setText("Switch to Reading Mode (Alt)");
        }
    });

    // Load the default style sheet
    QFile file(get_style_path().c_str());
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = file.readAll();
        app.setStyleSheet(styleSheet);
        file.close();
    }

    KeyPressFilter *filter = new KeyPressFilter(toggleButton, saveButton);
    window.installEventFilter(filter);
    window.setLayout(layout);
    window.resize(600, 400);
    window.show();

    // At last, get entries which will prompt for password
    getEntries();

    return app.exec();
}
