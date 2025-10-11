#include "mapped_file.h"

#ifdef _WIN32
#include <windows.h>
#include <cstdio>

static void log_last_error(const char* where) {
    DWORD e = GetLastError();
    char buf[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, e, 0, buf, sizeof(buf), nullptr);
    std::fprintf(stderr, "%s failed: (%lu) %s\n", where, (unsigned long)e, buf);
}

bool MappedFile::open(const std::string& path) {
    // Match _SH_DENYNO: allow others to read/write/delete while we map
    hFile = CreateFileA(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) { hFile = nullptr; log_last_error("CreateFileA"); return false; }

    LARGE_INTEGER fs;
    if (!GetFileSizeEx((HANDLE)hFile, &fs)) { log_last_error("GetFileSizeEx"); close(); return false; }
    size = static_cast<std::size_t>(fs.QuadPart);

    hMap = CreateFileMappingA((HANDLE)hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap) { log_last_error("CreateFileMapping"); close(); return false; }

    data = static_cast<const unsigned char*>(
        MapViewOfFile((HANDLE)hMap, FILE_MAP_READ, 0, 0, 0));
    if (!data) { log_last_error("MapViewOfFile"); close(); return false; }

    return true;
}

void MappedFile::close() {
    if (data) { UnmapViewOfFile((LPCVOID)data); data = nullptr; }
    if (hMap) { CloseHandle((HANDLE)hMap); hMap = nullptr; }
    if (hFile) { CloseHandle((HANDLE)hFile); hFile = nullptr; }
    size = 0;
}

#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>

bool MappedFile::open(const std::string& path) {
    fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) { std::fprintf(stderr, "open(%s) failed: %d\n", path.c_str(), errno); return false; }

    struct stat st {};
    if (fstat(fd, &st) < 0) { std::fprintf(stderr, "fstat failed: %d\n", errno); close(); return false; }
    size = static_cast<std::size_t>(st.st_size);

    void* p = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { std::fprintf(stderr, "mmap failed: %d\n", errno); close(); return false; }
    data = static_cast<const unsigned char*>(p);

    // advise sequential to help readahead
    posix_madvise((void*)data, size, POSIX_MADV_SEQUENTIAL);
    return true;
}

void MappedFile::close() {
    if (data) { munmap((void*)data, size); data = nullptr; }
    if (fd >= 0) { ::close(fd); fd = -1; }
    size = 0;
}
#endif
