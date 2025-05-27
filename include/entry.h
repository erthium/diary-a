#ifndef ENTRY_H
#define ENTRY_H

#include <vector>
#include <string>
#include <QString>

struct Entry {
    std::string content;
    std::string date;
};

// Configuration related functions
std::string get_conf(std::string key);
const std::string get_user_data_dir();
const std::string get_entry_dir();
const std::string get_styles_dir();
const std::string get_style();
const std::string get_style_path();
const std::string get_public_key_path();
const std::string get_private_key_path();

// Entry management functions
void getEntries();
void logEntry(const std::string &content);
QString parseMarkdown(const QString &text);

extern std::vector<Entry> entries;

#endif // ENTRY_H
