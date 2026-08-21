#include <iostream>
#include <vector>
#include "storage/buffer_pool.h"

int main() {
    const std::string db_filename = "vector_engine_test.db";
    
    // Step 1: Initialize Buffer Pool & Create persistent data page
    uint32_t target_page_id = 0;
    int32_t target_slot_id = -1;

    {
        storage::BufferPoolManager bpm(db_filename, 10);
        target_page_id = bpm.new_page();

        storage::SlottedPage* page = bpm.fetch_page(target_page_id);
        
        std::vector<float> sample_vector = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
        uint16_t bytes = sample_vector.size() * sizeof(float);

        target_slot_id = page->insert_record(reinterpret_cast<const uint8_t*>(sample_vector.data()), bytes);
        bpm.unpin_page(target_page_id, true);
        bpm.flush_page(target_page_id);

        std::cout << "[Write] Flushed Page ID: " << target_page_id 
                  << " with Slot ID: " << target_slot_id << " to " << db_filename << std::endl;
    } // BufferPoolManager closes handle here

    // Step 2: Re-open file with new BufferPool instance and verify disk page persistence
    {
        storage::BufferPoolManager bpm(db_filename, 10);
        storage::SlottedPage* page = bpm.fetch_page(target_page_id);

        std::vector<uint8_t> read_bytes;
        if (page->get_record(target_slot_id, read_bytes)) {
            const float* float_data = reinterpret_cast<const float*>(read_bytes.data());
            std::cout << "[Read] Reloaded page from disk successfully!" << std::endl;
            std::cout << "Values read: " << float_data[0] << ", " << float_data[1] << ", " 
                      << float_data[2] << ", " << float_data[3] << ", " << float_data[4] << std::endl;
        } else {
            std::cerr << "Failed to deserialize vector record from disk page!" << std::endl;
        }
    }

    return 0;
}