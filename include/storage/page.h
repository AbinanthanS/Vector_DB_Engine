#ifndef PAGE_H
#define PAGE_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <stdexcept>

namespace storage {

constexpr size_t PAGE_SIZE = 4096; // Fixed 4KB Page size

// Slot header tracking record placement within a page
struct Slot {
    uint16_t offset; // Offset from start of page where payload begins
    uint16_t length; // Length of data payload in bytes
};

// Slotted Page Header layout located at the very start of the 4KB buffer
struct PageHeader {
    uint16_t slot_count;          // Number of allocated slots
    uint16_t free_space_pointer;  // Grows down from 4096 toward slot array
};

class SlottedPage {
public:
    SlottedPage();
    
    // Low-level buffer access
    uint8_t* raw_data() { return data_; }
    const uint8_t* raw_data() const { return data_; }

    // Insert raw binary payload (vector + metadata byte payload)
    int32_t insert_record(const uint8_t* payload, uint16_t length);
    
    // Retrieve payload by slot index
    bool get_record(uint16_t slot_id, std::vector<uint8_t>& out_payload) const;

    // Available contiguous space for new records
    size_t get_free_space() const;

private:
    uint8_t data_[PAGE_SIZE];

    PageHeader* header() { return reinterpret_cast<PageHeader*>(data_); }
    const PageHeader* header() const { return reinterpret_cast<const PageHeader*>(data_); }
    
    Slot* slots() { return reinterpret_cast<Slot*>(data_ + sizeof(PageHeader)); }
    const Slot* slots() const { return reinterpret_cast<const Slot*>(data_ + sizeof(PageHeader)); }
};

} // namespace storage

#endif // PAGE_H