#include <crypto.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <fstream>
#include <iostream>
#include <vector>

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
