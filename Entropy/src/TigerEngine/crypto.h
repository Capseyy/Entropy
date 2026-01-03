#pragma once
#include <cstdint>
#include <openssl/evp.h>
#include <string>


extern const unsigned char AES_KEY_0[16];
extern const unsigned char AES_KEY_1[16];


void CryptoInit();
void CryptoCleanup();








bool AESGCM_Decrypt(const unsigned char* key,
    const unsigned char* nonce,
    const unsigned char* gcmTag,
    unsigned char* buffer,
    size_t size,
    const std::string& pname);