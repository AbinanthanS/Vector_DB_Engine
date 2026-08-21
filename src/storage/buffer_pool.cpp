#include "storage/buffer_pool.h"
#include <iostream>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

namespace storage {

BufferPoolManager::BufferPoolManager(const std::string& db_file_path, size_t pool_capacity)
    : file_path_(db_file_path), pool_capacity_(pool_capacity), next_page_id_(0) {
#ifdef _WIN32
    file_handle_ = INVALID_HANDLE_VALUE;
    file_mapping_handle_ = NULL;
#else
    file_descriptor_ = -1;
#endif
    init_file();
}

BufferPoolManager::~BufferPoolManager() {
    // Flush all dirty pages to disk on shutdown
    for (auto& [page_id, is_dirty] : dirty_flags_) {
        if (is_dirty) {
            flush_page(page_id);
        }
    }
    close_file();
}

void BufferPoolManager::init_file() {
#ifdef _WIN32
    file_handle_ = CreateFileA(
        file_path_.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (file_handle_ == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to open or create DB file on Windows.");
    }

    LARGE_INTEGER file_size;
    GetFileSizeEx(file_handle_, &file_size);
    next_page_id_ = static_cast<uint32_t>(file_size.QuadPart / PAGE_SIZE);

#else
    file_descriptor_ = open(file_path_.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (file_descriptor_ < 0) {
        throw std::runtime_error("Failed to open or create DB file on POSIX.");
    }

    struct stat st;
    if (fstat(file_descriptor_, &st) == 0) {
        next_page_id_ = static_cast<uint32_t>(st.st_size / PAGE_SIZE);
    }
#endif
}

void BufferPoolManager::close_file() {
#ifdef _WIN32
    if (file_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_handle_);
        file_handle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (file_descriptor_ >= 0) {
        close(file_descriptor_);
        file_descriptor_ = -1;
    }
#endif
}

uint32_t BufferPoolManager::new_page() {
    uint32_t page_id = next_page_id_++;
    SlottedPage page;
    page_table_[page_id] = page;
    dirty_flags_[page_id] = true;
    return page_id;
}

SlottedPage* BufferPoolManager::fetch_page(uint32_t page_id) {
    // Check if page already resides in RAM buffer
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        return &it->second;
    }

    // Load page from disk file into RAM pool
    SlottedPage page;
    read_page_from_disk(page_id, page);
    page_table_[page_id] = page;
    dirty_flags_[page_id] = false;
    return &page_table_[page_id];
}

bool BufferPoolManager::flush_page(uint32_t page_id) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return false;

    write_page_to_disk(page_id, it->second);
    dirty_flags_[page_id] = false;
    return true;
}

void BufferPoolManager::unpin_page(uint32_t page_id, bool is_dirty) {
    if (is_dirty) {
        dirty_flags_[page_id] = true;
    }
}

void BufferPoolManager::write_page_to_disk(uint32_t page_id, const SlottedPage& page) {
    uint64_t offset = static_cast<uint64_t>(page_id) * PAGE_SIZE;

#ifdef _WIN32
    OVERLAPPED overlapped = {0};
    overlapped.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
    overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);

    DWORD bytes_written;
    if (!WriteFile(file_handle_, page.raw_data(), PAGE_SIZE, &bytes_written, &overlapped)) {
        throw std::runtime_error("Failed to write 4KB page to disk on Windows.");
    }
#else
    ssize_t bytes_written = pwrite(file_descriptor_, page.raw_data(), PAGE_SIZE, offset);
    if (bytes_written != static_cast<ssize_t>(PAGE_SIZE)) {
        throw std::runtime_error("Failed to write 4KB page to disk on POSIX.");
    }
#endif
}

void BufferPoolManager::read_page_from_disk(uint32_t page_id, SlottedPage& page) {
    uint64_t offset = static_cast<uint64_t>(page_id) * PAGE_SIZE;

#ifdef _WIN32
    OVERLAPPED overlapped = {0};
    overlapped.Offset = static_cast<DWORD>(offset & 0xFFFFFFFF);
    overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);

    DWORD bytes_read;
    if (!ReadFile(file_handle_, page.raw_data(), PAGE_SIZE, &bytes_read, &overlapped)) {
        throw std::runtime_error("Failed to read 4KB page from disk on Windows.");
    }
#else
    ssize_t bytes_read = pread(file_descriptor_, page.raw_data(), PAGE_SIZE, offset);
    if (bytes_read != static_cast<ssize_t>(PAGE_SIZE)) {
        throw std::runtime_error("Failed to read 4KB page from disk on POSIX.");
    }
#endif
}

} // namespace storage