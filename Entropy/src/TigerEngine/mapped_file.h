#pragma once
#include <cstddef>
#include <string>

struct MappedFile {
    std::size_t size = 0;
    const unsigned char* data = nullptr;

#ifdef _WIN32
    void* hFile = nullptr;   // HANDLE
    void* hMap = nullptr;   // HANDLE
    bool open(const std::string& path);
    void close();
    ~MappedFile() { close(); }
#else
    int fd = -1;
    bool open(const std::string& path);
    void close();
    ~MappedFile() { close(); }
#endif
};
