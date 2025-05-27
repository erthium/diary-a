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
#include <QFile>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <iostream>
#include <sstream>
#include <filesystem>


class KeyPressFilter : public QObject {
  public:
    KeyPressFilter(QPushButton *toggle_button, QPushButton *enter_button) : toggle_button(toggle_button), enter_button(enter_button) {}

    bool eventFilter(QObject *obj, QEvent *event) override {
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

  private:
    QPushButton *toggle_button;
    QPushButton *enter_button;
  };

struct Entry {
  std::string content;
  std::string date;
};

const std::string CONFIG_FILE = "/etc/diary.conf";
std::string get_conf(std::string key) {
  std::ifstream config_file(CONFIG_FILE);
  std::string line;
  while (std::getline(config_file, line)) {
    if (line.substr(0, key.size()) == key && (line[key.size()] == '=' || line[key.size()] == ' ')) {
      std::string value = line.substr(key.size() + 1);
      // replace ~ with home directory if found
      if (value[0] == '~') {
        value.replace(0, 1, std::getenv("HOME"));
      }
      return value;
    }
  }
  return std::string(std::getenv("HOME")) + "/.diary_a"; // Default entry directory if not found
}

const std::string user_data_dir = get_conf("user_data_dir");
const std::string entry_dir = get_conf("entry_dir");
const std::string styles_dir = get_conf("styles_dir");
const std::string style = get_conf("style");
const std::string style_path = styles_dir + "/" + style;
const std::string public_key_path = get_conf("public_key");
const std::string private_key_path = get_conf("private_key");


std::vector<Entry> entries;


bool encrypt_data(const std::string &input_text, const std::string &output_file, const std::string &public_key_path) {
  // Generate a random AES key
  unsigned char aes_key[32];  // 256-bit key
  if (RAND_bytes(aes_key, sizeof(aes_key)) != 1) {
    std::cerr << "Error generating random key.\n";
    return false;
  }

  // Initialize AES encryption
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    std::cerr << "Error creating cipher context.\n";
    return false;
  }

  // Generate random IV
  unsigned char iv[16];
  if (RAND_bytes(iv, sizeof(iv)) != 1) {
    std::cerr << "Error generating IV.\n";
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }

  // Initialize encryption
  if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, aes_key, iv) != 1) {
    std::cerr << "Error initializing encryption.\n";
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }

  // Encrypt the data
  std::vector<unsigned char> encrypted(input_text.size() + EVP_MAX_BLOCK_LENGTH);
  int len1, len2;
  if (EVP_EncryptUpdate(ctx, encrypted.data(), &len1, 
                       (unsigned char*)input_text.c_str(), input_text.size()) != 1) {
    std::cerr << "Error encrypting data.\n";
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }

  if (EVP_EncryptFinal_ex(ctx, encrypted.data() + len1, &len2) != 1) {
    std::cerr << "Error finalizing encryption.\n";
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }
  encrypted.resize(len1 + len2);
  EVP_CIPHER_CTX_free(ctx);

  // Load RSA public key
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

  // Encrypt the AES key with RSA
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new(pkey, nullptr);
  if (!pctx || EVP_PKEY_encrypt_init(pctx) <= 0) {
    std::cerr << "Error initializing RSA encryption.\n";
    EVP_PKEY_free(pkey);
    return false;
  }

  size_t outlen;
  EVP_PKEY_encrypt(pctx, nullptr, &outlen, aes_key, sizeof(aes_key));

  std::vector<unsigned char> encrypted_key(outlen);
  if (EVP_PKEY_encrypt(pctx, encrypted_key.data(), &outlen, aes_key, sizeof(aes_key)) <= 0) {
    std::cerr << "Error encrypting AES key.\n";
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pctx);
    return false;
  }

  EVP_PKEY_free(pkey);
  EVP_PKEY_CTX_free(pctx);

  // Write the encrypted data to file
  std::ofstream output(output_file, std::ios::binary);
  if (!output) {
    std::cerr << "Error opening output file.\n";
    return false;
  }

  // Write IV length and IV
  uint32_t iv_len = sizeof(iv);
  output.write(reinterpret_cast<char*>(&iv_len), sizeof(iv_len));
  output.write(reinterpret_cast<char*>(iv), iv_len);

  // Write encrypted key length and encrypted key
  uint32_t key_len = encrypted_key.size();
  output.write(reinterpret_cast<char*>(&key_len), sizeof(key_len));
  output.write(reinterpret_cast<char*>(encrypted_key.data()), key_len);

  // Write encrypted data length and encrypted data
  uint32_t data_len = encrypted.size();
  output.write(reinterpret_cast<char*>(&data_len), sizeof(data_len));
  output.write(reinterpret_cast<char*>(encrypted.data()), data_len);

  return true;
}


bool decrypt_data(const std::string &input_file, std::string &output_text, const std::string &private_key_path, const std::string &password) {
  std::ifstream input(input_file, std::ios::binary);
  if (!input) {
    std::cerr << "Error opening input file.\n";
    return false;
  }

  // Read IV
  uint32_t iv_len;
  input.read(reinterpret_cast<char*>(&iv_len), sizeof(iv_len));
  std::vector<unsigned char> iv(iv_len);
  input.read(reinterpret_cast<char*>(iv.data()), iv_len);

  // Read encrypted key
  uint32_t key_len;
  input.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
  std::vector<unsigned char> encrypted_key(key_len);
  input.read(reinterpret_cast<char*>(encrypted_key.data()), key_len);

  // Read encrypted data
  uint32_t data_len;
  input.read(reinterpret_cast<char*>(&data_len), sizeof(data_len));
  std::vector<unsigned char> encrypted_data(data_len);
  input.read(reinterpret_cast<char*>(encrypted_data.data()), data_len);

  // Load RSA private key
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

  // Decrypt the AES key
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new(pkey, nullptr);
  if (!pctx || EVP_PKEY_decrypt_init(pctx) <= 0) {
    std::cerr << "Error initializing RSA decryption.\n";
    EVP_PKEY_free(pkey);
    return false;
  }

  size_t outlen;
  EVP_PKEY_decrypt(pctx, nullptr, &outlen, encrypted_key.data(), encrypted_key.size());

  std::vector<unsigned char> aes_key(outlen);
  if (EVP_PKEY_decrypt(pctx, aes_key.data(), &outlen, encrypted_key.data(), encrypted_key.size()) <= 0) {
    std::cerr << "Error decrypting AES key.\n";
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pctx);
    return false;
  }

  EVP_PKEY_free(pkey);
  EVP_PKEY_CTX_free(pctx);

  // Initialize AES decryption
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    std::cerr << "Error creating cipher context.\n";
    return false;
  }

  if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, aes_key.data(), iv.data()) != 1) {
    std::cerr << "Error initializing decryption.\n";
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }

  // Decrypt the data
  std::vector<unsigned char> decrypted(encrypted_data.size());
  int len1, len2;
  if (EVP_DecryptUpdate(ctx, decrypted.data(), &len1, encrypted_data.data(), encrypted_data.size()) != 1) {
    std::cerr << "Error decrypting data.\n";
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }

  if (EVP_DecryptFinal_ex(ctx, decrypted.data() + len1, &len2) != 1) {
    std::cerr << "Error finalizing decryption.\n";
    EVP_CIPHER_CTX_free(ctx);
    return false;
  }

  decrypted.resize(len1 + len2);
  output_text = std::string(decrypted.begin(), decrypted.end());

  EVP_CIPHER_CTX_free(ctx);
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
  QFile file(style_path.c_str());
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
