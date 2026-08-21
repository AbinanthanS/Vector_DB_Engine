#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include "storage/page.h"

namespace storage {

class BufferPoolManager {
public:
    BufferPoolManager(const std::string& db_file_path, size_t pool_capacity);
    ~BufferPoolManager();

    // Fetch page from RAM cache or load from mapped disk storage
    SlottedPage* fetch_page(uint32_t page_id);

    // Flush dirty page modifications out to disk file
    bool flush_page(uint32_t page_id);

    // Allocate a brand-new page on disk
    uint32_t new_page();

    // Unpin page from LRU cache tracking
    void unpin_page(uint32_t page_id, bool is_dirty);

private:
    std::string file_path_;
    size_t pool_capacity_;
    uint32_t next_page_id_;

    // In-Memory Cache Structures
    std::unordered_map<uint32_t, SlottedPage> page_table_;
    std::unordered_map<uint32_t, bool> dirty_flags_;

    // OS Native Storage Handles
#ifdef _WIN32
    void* file_handle_;
    void* file_mapping_handle_;
#else
    int file_descriptor_;
#endif

    void init_file();
    void close_file();
    void write_page_to_disk(uint32_t page_id, const SlottedPage& page);
    void read_page_from_disk(uint32_t page_id, SlottedPage& page);
};

} // namespace storage

#endif // BUFFER_POOL_H