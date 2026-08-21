#ifndef BTREE_H
#define BTREE_H

#include <cstdint>
#include <vector>
#include "storage/buffer_pool.h"

namespace index {

// Physical location pointer of a record inside the DB engine
struct RecordID {
    uint32_t page_id;
    uint16_t slot_id;

    bool operator==(const RecordID& other) const {
        return page_id == other.page_id && slot_id == other.slot_id;
    }
};

constexpr size_t MAX_BTREE_KEYS = 3; // Kept small for testing node splits

struct BTreeNodePayload {
    bool is_leaf;
    uint16_t num_keys;
    uint32_t keys[MAX_BTREE_KEYS];
    
    // Internal node pointers to child page IDs
    uint32_t child_page_ids[MAX_BTREE_KEYS + 1];

    // Leaf node data pointers to physical record locations
    RecordID record_ids[MAX_BTREE_KEYS];
    
    // Pointer to next leaf node for fast range scans
    uint32_t next_leaf_page_id; 
};

class BTreeIndex {
public:
    BTreeIndex(storage::BufferPoolManager& bpm);

    // Insert a scalar key mapping to a RecordID
    void insert(uint32_t key, RecordID rid);

    // Search for matching RecordIDs by scalar key
    bool search(uint32_t key, std::vector<RecordID>& out_rids);

    uint32_t root_page_id() const { return root_page_id_; }

private:
    storage::BufferPoolManager& bpm_;
    uint32_t root_page_id_;

    void insert_non_full(uint32_t node_page_id, uint32_t key, RecordID rid);
    void split_child(uint32_t parent_page_id, int child_idx, uint32_t child_page_id);
};

} // namespace index

#endif // BTREE_H