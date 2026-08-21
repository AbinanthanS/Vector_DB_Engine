#include <iostream>
#include <vector>
#include "storage/page.h"

int main() {
    storage::SlottedPage page;

    // Create dummy 128-dim float vector payload
    std::vector<float> original_vector(128, 0.42f);
    uint16_t payload_bytes = original_vector.size() * sizeof(float);

    // Insert into page
    int32_t slot_id = page.insert_record(reinterpret_cast<const uint8_t*>(original_vector.data()), payload_bytes);
    std::cout << "Inserted 128-dim vector into Slot ID: " << slot_id << std::endl;
    std::cout << "Remaining page free space: " << page.get_free_space() << " bytes" << std::endl;

    // Read back from page
    std::vector<uint8_t> read_buffer;
    if (page.get_record(slot_id, read_buffer)) {
        const float* read_floats = reinterpret_cast<const float*>(read_buffer.data());
        std::cout << "Successfully retrieved vector. First element value: " << read_floats[0] << std::endl;
    } else {
        std::cerr << "Failed to read record!" << std::endl;
    }

    return 0;
}