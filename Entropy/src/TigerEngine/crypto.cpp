#include "crypto.h"
#include <openssl/core_names.h>
#include <thread>
#include <cstdio>
#include <map>
#include <array>



// Hard-coded AES keys (if they were global before)
const unsigned char AES_KEY_0[16] = {
    0xD6, 0x2A, 0xB2, 0xC1, 0x0C, 0xC0, 0x1B, 0xC5,
    0x35, 0xDB, 0x7B, 0x86, 0x55, 0xC7, 0xDC, 0x3B,
};
const unsigned char AES_KEY_1[16] = {
    0x3A, 0x4A, 0x5D, 0x36, 0x73, 0xA6, 0x60, 0x58,
    0x7E, 0x63, 0xE6, 0x76, 0xE4, 0x08, 0x92, 0xB5,
};

// Thread-local contexts (one per key flavor)
static thread_local EVP_CIPHER_CTX* tls_ctx0 = nullptr;
static thread_local EVP_CIPHER_CTX* tls_ctx1 = nullptr;
static thread_local EVP_CIPHER_CTX* tls_ctx_custom = nullptr;

void CryptoInit() {
    // OpenSSL self-init is automatic on 1.1.1+, no global init needed.
}

void CryptoCleanup() {
    if (tls_ctx0) { EVP_CIPHER_CTX_free(tls_ctx0); tls_ctx0 = nullptr; }
    if (tls_ctx1) { EVP_CIPHER_CTX_free(tls_ctx1); tls_ctx1 = nullptr; }
    if (tls_ctx_custom) { EVP_CIPHER_CTX_free(tls_ctx_custom); tls_ctx_custom = nullptr; }
}

static EVP_CIPHER_CTX* get_ctx(const unsigned char* key,
    EVP_CIPHER_CTX*& slot)
{
    if (!slot) {
        slot = EVP_CIPHER_CTX_new();
        EVP_DecryptInit_ex(slot, EVP_aes_128_gcm(), nullptr, nullptr, nullptr);
        EVP_CIPHER_CTX_ctrl(slot, EVP_CTRL_AEAD_SET_IVLEN, 12, nullptr);
        EVP_DecryptInit_ex(slot, nullptr, nullptr, key, nullptr);
    }
    return slot;
}

static thread_local std::map<std::array<unsigned char, 16>, EVP_CIPHER_CTX*> tls_ctx_map;

static EVP_CIPHER_CTX* get_ctx_for_key(const unsigned char* key)
{
    std::array<unsigned char, 16> k{};
    std::memcpy(k.data(), key, 16);

    auto it = tls_ctx_map.find(k);
    if (it != tls_ctx_map.end())
        return it->second;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, nullptr);
    tls_ctx_map[k] = ctx;
    return ctx;
}

bool AESGCM_Decrypt(const unsigned char* key,
    const unsigned char* nonce,
    const unsigned char* gcmTag,
    unsigned char* buffer,
    size_t size,
    const std::string& pname)
{
    // Pick correct thread-local context
    EVP_CIPHER_CTX* ctx =
        (key == AES_KEY_0) ? get_ctx_for_key(AES_KEY_0) :
        (key == AES_KEY_1) ? get_ctx_for_key(AES_KEY_1) :
        get_ctx_for_key(key);

    if (!ctx) return false;

    // Set IV for this block
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, nullptr, nonce) != 1) {
        std::fprintf(stderr, "EVP_DecryptInit_ex(IV) failed\n");
        return false;
    }

    int outlen = 0;
    if (EVP_DecryptUpdate(ctx, buffer, &outlen, buffer, (int)size) != 1) {
        std::fprintf(stderr, "EVP_DecryptUpdate failed\n");
        return false;
    }

    // Supply authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16, (void*)gcmTag) != 1) {
        std::fprintf(stderr, "EVP_CTRL_AEAD_SET_TAG failed\n");
        return false;
    }

    int fin = 0;
    if (EVP_DecryptFinal_ex(ctx, buffer + outlen, &fin) != 1) {
        std::fprintf(stderr, "EVP_DecryptFinal_ex (tag check) failed with key: ");
        for (int i = 0; i < 16; ++i)
            std::fprintf(stderr, "%02X", key[i]);
        std::fprintf(stderr, "and IV : ");
        for (int i = 0; i < 12; ++i)
            std::fprintf(stderr, "%02X", nonce[i]);
		std::fprintf(stderr, "\nFor GCMTAG: ");
        for (int i = 0; i < 16; ++i)
            std::fprintf(stderr, "%02X", gcmTag[i]);
        
		std::fprintf(stderr, " for package %s", pname.c_str());
        std::fprintf(stderr, "\n");
        return false;
    }

    return true;
}
