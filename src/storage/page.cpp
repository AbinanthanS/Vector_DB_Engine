#include "storage/page.h"

namespace storage {

SlottedPage::SlottedPage() {
    std::memset(data_, 0, PAGE_SIZE);
    PageHeader* hdr = header();
    hdr->slot_count = 0;
    hdr->free_space_pointer = PAGE_SIZE; // Free space starts at end of page
}

size_t SlottedPage::get_free_space() const {
    const PageHeader* hdr = header();
    size_t slots_end = sizeof(PageHeader) + (hdr->slot_count * sizeof(Slot));
    if (hdr->free_space_pointer < slots_end) return 0;
    return hdr->free_space_pointer - slots_end;
}

int32_t SlottedPage::insert_record(const uint8_t* payload, uint16_t length) {
    PageHeader* hdr = header();
    size_t space_needed = sizeof(Slot) + length;

    if (get_free_space() < space_needed) {
        return -1; // Page is full
    }

    // Allocate payload from bottom of page upwards
    hdr->free_space_pointer -= length;
    std::memcpy(data_ + hdr->free_space_pointer, payload, length);

    // Setup slot entry array at top of page
    uint16_t slot_id = hdr->slot_count;
    Slot* slot_array = slots();
    slot_array[slot_id].offset = hdr->free_space_pointer;
    slot_array[slot_id].length = length;

    hdr->slot_count++;
    return static_cast<int32_t>(slot_id);
}

bool SlottedPage::get_record(uint16_t slot_id, std::vector<uint8_t>& out_payload) const {
    const PageHeader* hdr = header();
    if (slot_id >= hdr->slot_count) return false;

    const Slot* slot_array = slots();
    const Slot& slot = slot_array[slot_id];

    if (slot.length == 0) return false; // Deleted or empty slot

    out_payload.resize(slot.length);
    std::memcpy(out_payload.data(), data_ + slot.offset, slot.length);
    return true;
}

} 