#include <iostream>
#include <vector>
#include "storage/buffer_pool.h"
#include "index/btree.h"

int main() {
    const std::string db_file = "btree_test.db";
    storage::BufferPoolManager bpm(db_file, 10);
    index::BTreeIndex btree(bpm);

    // Insert scalar metadata mapping to dummy record IDs
    index::RecordID rid1{1, 0};
    index::RecordID rid2{2, 1};
    index::RecordID rid3{3, 2};

    btree.insert(100, rid1);
    btree.insert(200, rid2);
    btree.insert(300, rid3);

    std::cout << "[B+Tree] Inserted scalar keys (100, 200, 300) into disk pages." << std::endl;

    // Query key 200
    std::vector<index::RecordID> results;
    if (btree.search(200, results)) {
        std::cout << "[B+Tree Search] Key 200 Found -> Page ID: " 
                  << results[0].page_id << ", Slot ID: " << results[0].slot_id << std::endl;
    } else {
        std::cerr << "[B+Tree Search] Key 200 Not Found!" << std::endl;
    }

    return 0;
}