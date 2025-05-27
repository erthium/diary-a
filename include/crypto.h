#ifndef CRYPTO_H
#define CRYPTO_H

#include <string>

bool encrypt_data(const std::string &input_text, const std::string &output_file, const std::string &public_key_path);
bool decrypt_data(const std::string &input_file, std::string &output_text, const std::string &private_key_path, const std::string &password);

#endif // CRYPTO_H
