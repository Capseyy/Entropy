#pragma once
#include <cstdint>
#include <openssl/evp.h>
#include <string>

// 16-byte AES-128 keys
extern const unsigned char AES_KEY_0[16];
extern const unsigned char AES_KEY_1[16];

// Initialize / cleanup (optional)
void CryptoInit();
void CryptoCleanup();

// Decrypt a single GCM block (in-place).
//  key      : pointer to 16-byte key (AES-128)
//  nonce    : 12-byte IV
//  gcmTag   : 16-byte authentication tag
//  buffer   : ciphertext + plaintext (same buffer, modified in place)
//  size     : payload size in bytes
// Returns true if tag verified successfully.
bool AESGCM_Decrypt(const unsigned char* key,
    const unsigned char* nonce,
    const unsigned char* gcmTag,
    unsigned char* buffer,
    size_t size,
    std::string pname);