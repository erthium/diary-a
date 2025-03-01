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
#include <QInputDialog>


#include <openssl/evp.h>
#include <openssl/pem.h>

#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <iostream>
#include <sstream>
#include <filesystem>


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

const std::string CONFIG_FILE = "/etc/diary.conf";
std::string get_entry_directory() {
  std::ifstream config_file(CONFIG_FILE);
  std::string line;
  while (std::getline(config_file, line)) {
    if (line.find("user_data_dir=") == 0) {
      std::string dir = line.substr(14);
      // replace ~ with home directory if found
      if (dir[0] == '~') {
        dir.replace(0, 1, std::getenv("HOME"));
      }
      return dir;
    }
  }
  return std::string(std::getenv("HOME")) + "/.diary_a"; // Default entry directory if not found
}

const std::string user_data_dir = get_entry_directory();
const std::string entry_dir = user_data_dir + "/entries";
const std::string public_key_path = user_data_dir + "/public_key.pem";
const std::string private_key_path = user_data_dir + "/private_key.pem";
std::vector<Entry> entries;


bool encrypt_data(const std::string &input_text, const std::string &output_file, const std::string &public_key_path) {
  FILE *pub_file = fopen(public_key_path.c_str(), "rb");
  if (!pub_file) {
    std::cerr << "Error opening public key file.\n";
    return false;
  }

  EVP_PKEY *pkey = PEM_read_PUBKEY(pub_file, nullptr, nullptr, nullptr);
  fclose(pub_file);
  if (!pkey) {
    std::cerr << "Error reading public key.\n";
    return false;
  }

  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, nullptr);
  if (!ctx || EVP_PKEY_encrypt_init(ctx) <= 0) {
    std::cerr << "Error initializing encryption.\n";
    EVP_PKEY_free(pkey);
    return false;
  }

  size_t outlen;
  EVP_PKEY_encrypt(ctx, nullptr, &outlen, (unsigned char*)input_text.c_str(), input_text.size());

  std::vector<unsigned char> encrypted(outlen);
  if (EVP_PKEY_encrypt(ctx, encrypted.data(), &outlen, (unsigned char*)input_text.c_str(), input_text.size()) <= 0) {
    std::cerr << "Error encrypting data.\n";
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);
    return false;
  }

  std::ofstream output(output_file, std::ios::binary);
  output.write((char*)encrypted.data(), outlen);
  
  EVP_PKEY_free(pkey);
  EVP_PKEY_CTX_free(ctx);
  return true;
}


bool decrypt_data(const std::string &input_file, std::string &output_text, const std::string &private_key_path, const std::string &password) {
  FILE *priv_file = fopen(private_key_path.c_str(), "rb");
  if (!priv_file) {
    std::cerr << "Error opening private key file.\n";
    return false;
  }

  EVP_PKEY *pkey = PEM_read_PrivateKey(priv_file, nullptr, nullptr, (void*)password.c_str());
  fclose(priv_file);
  if (!pkey) {
    std::cerr << "Error reading private key.\n";
    return false;
  }

  std::ifstream input(input_file, std::ios::binary);
  std::vector<unsigned char> encrypted((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, nullptr);
  if (!ctx || EVP_PKEY_decrypt_init(ctx) <= 0) {
    std::cerr << "Error initializing decryption.\n";
    EVP_PKEY_free(pkey);
    return false;
  }

  size_t outlen;
  EVP_PKEY_decrypt(ctx, nullptr, &outlen, encrypted.data(), encrypted.size());

  std::vector<unsigned char> decrypted(outlen);
  if (EVP_PKEY_decrypt(ctx, decrypted.data(), &outlen, encrypted.data(), encrypted.size()) <= 0) {
    std::cerr << "Error decrypting data.\n";
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);
    return false;
  }

  output_text = std::string(decrypted.begin(), decrypted.end());

  EVP_PKEY_free(pkey);
  EVP_PKEY_CTX_free(ctx);
  return true;
}


std::string get_password_popup() {
  bool ok;
  QString password = QInputDialog::getText(nullptr, "Enter Password", "Password:", QLineEdit::Password, "", &ok);
  if (ok) {
    return password.toStdString();
  }
  // if the user cancels, exit the program
  exit(0);
}


void getEntries() {
  // get all files, decrypt them and add them to entries
  std::ifstream file;
  std::string line;
  std::string file_path;
  std::string content;
  std::string password = get_password_popup();
  for (const auto &entry : std::filesystem::directory_iterator(entry_dir)) {
    file_path = entry.path().string();
    if (!decrypt_data(file_path, content, private_key_path, password)) {
      exit(1);
    }
    std::istringstream ss(content);
    Entry new_entry;
    std::getline(ss, line);
    new_entry.date = line;
    new_entry.content = "";
    while (std::getline(ss, line)) {
      new_entry.content += line + "\n";
    }

    entries.push_back(new_entry);
  }
  password = "";

  // sort entries by date
  std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
    return a.date > b.date;
  });
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
  std::string file_path = entry_dir + "/" + std::to_string(now_c) + ".txt";
  std::string input_text = date + content;
  encrypt_data(input_text, file_path, public_key_path);

  // add entry to entries, beginning of vector
  entries.insert(entries.begin(), entry);
}


QString parseMarkdown(const QString &text) {
  QString formatted = text;
  formatted.replace("**", "<b>").replace("**", "</b>");  // Bold
  formatted.replace("*", "<i>").replace("*", "</i>");    // Italic
  return formatted;
}


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
  #pragma region Writing Mode

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

  window.setLayout(layout);
  window.resize(600, 400);

  KeyPressFilter *filter = new KeyPressFilter(toggleButton);
  window.installEventFilter(filter);
  window.show();
  getEntries();

  return app.exec();
}
