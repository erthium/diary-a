#include <entry.h>
#include <crypto.h>

#include <QInputDialog>
#include <QLineEdit>

#include <fstream>
#include <chrono>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <algorithm>

std::vector<Entry> entries;

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

const std::string get_user_data_dir() { return get_conf("user_data_dir"); }
const std::string get_entry_dir() { return get_conf("entry_dir"); }
const std::string get_styles_dir() { return get_conf("styles_dir"); }
const std::string get_style() { return get_conf("style"); }
const std::string get_style_path() { return get_styles_dir() + "/" + get_style(); }
const std::string get_public_key_path() { return get_conf("public_key"); }
const std::string get_private_key_path() { return get_conf("private_key"); }

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
    for (const auto &entry : std::filesystem::directory_iterator(get_entry_dir())) {
        file_path = entry.path().string();
        if (!decrypt_data(file_path, content, get_private_key_path(), password)) {
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
    std::string file_path = get_entry_dir() + "/" + std::to_string(now_c) + ".txt";
    std::string input_text = date + content;
    encrypt_data(input_text, file_path, get_public_key_path());

    // add entry to entries, beginning of vector
    entries.insert(entries.begin(), entry);
}

QString parseMarkdown(const QString &text) {
    QString formatted = text;
    formatted.replace("**", "<b>").replace("**", "</b>");  // Bold
    formatted.replace("*", "<i>").replace("*", "</i>");    // Italic
    return formatted;
}
