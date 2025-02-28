#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>
#include <QScrollArea>
#include <QKeyEvent>
#include <QFont>

#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <iostream>


/*
entries will be saved in txt format in the entries directory
format:
datetime
content

first will always be the datetime, then the content
each entry will have its own file
*/

class KeyPressFilter : public QObject {
  public:
    KeyPressFilter(QPushButton *button) : button(button) {}

    bool eventFilter(QObject *obj, QEvent *event) override {
      if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
          static_cast<QWidget *>(obj)->close();  // Close the window when ESC is pressed
          return true;  // Event handled
        } else if (keyEvent->key() == Qt::Key_Alt) {
          button->click();  // Simulate a button click when D is pressed
          return true;  // Event handled
        }
      }
      return QObject::eventFilter(obj, event);  // Default event processing
    }

  private:
    QPushButton *button;
  };

struct Entry {
  std::string content;
  std::string date;
};

const std::string entry_dir = "";
std::vector<Entry> entries;


void getEntries() {
  // get all files in the entries directory
  std::string cmd = "ls " + entry_dir;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    std::cerr << "Error: popen failed" << std::endl;
    return;
  }

  char buffer[128];
  std::string result = "";
  while (!feof(pipe)) {
    if (fgets(buffer, 128, pipe) != NULL) {
      result += buffer;
    }
  }
  pclose(pipe);

  // parse result
  std::string entry;
  for (char c : result) {
    if (c == '\n') {
      // get entry
      std::ifstream file;
      file.open(entry_dir + "/" + entry);
      std::string date, content;
      std::getline(file, date);
      std::getline(file, content);
      file.close();

      // create entry
      Entry e;
      e.date = date;
      e.content = content;

      // add entry to entries
      entries.push_back(e);

      // reset entry
      entry = "";
    } else {
      entry += c;
    }
  }
}


void logEntry(const std::string &content) {
  // get current time
  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);
  std::string date = std::ctime(&now_c);

  // create entry
  Entry entry;
  entry.content = content;
  entry.date = date;

  // save entry
  std::ofstream file;
  file.open(entry_dir + "/" + std::to_string(now_c) + ".txt");
  file << date << content;
  file.close();

  // add entry to entries
  entries.push_back(entry);
}


QString parseMarkdown(const QString &text) {
  QString formatted = text;
  formatted.replace("**", "<b>").replace("**", "</b>");  // Bold
  formatted.replace("*", "<i>").replace("*", "</i>");    // Italic
  return formatted;
}


int main(int argc, char *argv[]) {
  getEntries();

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
  #pragma region Writing Mode

  QWidget *writeMode = new QWidget;
  QVBoxLayout *writeLayout = new QVBoxLayout(writeMode);

  QTextEdit *textEdit = new QTextEdit;
  textEdit->setPlaceholderText("Write your entry here...");
  textEdit->setFontPointSize(16);
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

  #pragma endregion

  // Reading mode (QLabel)
  #pragma region Reading Mode

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

  #pragma endregion

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
        QVBoxLayout *entryLayout = new QVBoxLayout(entryWidget);
        QLabel *dateLabel = new QLabel(QString::fromStdString(entry.date));
        entryLayout->addWidget(dateLabel);
        QLabel *entryLabel = new QLabel(parseMarkdown(QString::fromStdString(entry.content)));
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

  //window.setAttribute(Qt::WA_KeyCompression);  // Ensure key events are delivered

  window.setLayout(layout);
  window.resize(600, 400);

  KeyPressFilter *filter = new KeyPressFilter(toggleButton);
  window.installEventFilter(filter);
  window.show();

  return app.exec();
}
