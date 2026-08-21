#include "index/btree.h"
#include <cstring>
#include <iostream>

namespace vdb_index {

BTreeIndex::BTreeIndex(storage::BufferPoolManager& bpm) : bpm_(bpm) {
    // Allocate root node on a fresh 4KB page
    root_page_id_ = bpm_.new_page();
    storage::SlottedPage* page = bpm_.fetch_page(root_page_id_);

    BTreeNodePayload root_node{};
    root_node.is_leaf = true;
    root_node.num_keys = 0;
    root_node.next_leaf_page_id = 0xFFFFFFFF; // Null pointer

    page->insert_record(reinterpret_cast<const uint8_t*>(&root_node), sizeof(BTreeNodePayload));
    bpm_.unpin_page(root_page_id_, true);
    bpm_.flush_page(root_page_id_);
}

bool BTreeIndex::search(uint32_t key, std::vector<RecordID>& out_rids) {
    uint32_t current_page_id = root_page_id_;

    // Descend to the first leaf that can contain the key.
    while (true) {
        storage::SlottedPage* page = bpm_.fetch_page(current_page_id);
        std::vector<uint8_t> buffer;
        if (!page->get_record(0, buffer)) return false;

        BTreeNodePayload node{};
        std::memcpy(&node, buffer.data(), sizeof(BTreeNodePayload));

        if (node.is_leaf) break;

        int i = node.num_keys - 1;
        while (i >= 0 && key <= node.keys[i]) --i;
        current_page_id = node.child_page_ids[i + 1];
    }

    // B+ tree leaves are linked, so collect ALL duplicate-key records.
    bool found = false;
    while (current_page_id != 0xFFFFFFFF) {
        storage::SlottedPage* page = bpm_.fetch_page(current_page_id);
        std::vector<uint8_t> buffer;
        if (!page->get_record(0, buffer)) break;

        BTreeNodePayload node{};
        std::memcpy(&node, buffer.data(), sizeof(BTreeNodePayload));

        for (uint16_t i = 0; i < node.num_keys; ++i) {
            if (node.keys[i] == key) {
                out_rids.push_back(node.record_ids[i]);
                found = true;
            } else if (node.keys[i] > key) {
                return found;
            }
        }

        current_page_id = node.next_leaf_page_id;
    }

    return found;
}

void BTreeIndex::insert(uint32_t key, RecordID rid) {
    storage::SlottedPage* root_page = bpm_.fetch_page(root_page_id_);
    std::vector<uint8_t> buffer;
    root_page->get_record(0, buffer);
    BTreeNodePayload root_node;
    std::memcpy(&root_node, buffer.data(), sizeof(BTreeNodePayload));

    // If root is full, split it and increase tree height
    if (root_node.num_keys == MAX_BTREE_KEYS) {
        uint32_t new_root_page_id = bpm_.new_page();
        storage::SlottedPage* new_root_page = bpm_.fetch_page(new_root_page_id);

        BTreeNodePayload new_root{};
        new_root.is_leaf = false;
        new_root.num_keys = 0;
        new_root.child_page_ids[0] = root_page_id_;

        new_root_page->insert_record(reinterpret_cast<const uint8_t*>(&new_root), sizeof(BTreeNodePayload));
        bpm_.unpin_page(new_root_page_id, true);

        split_child(new_root_page_id, 0, root_page_id_);
        root_page_id_ = new_root_page_id;

        insert_non_full(root_page_id_, key, rid);
    } else {
        insert_non_full(root_page_id_, key, rid);
    }
}

void BTreeIndex::split_child(uint32_t parent_page_id, int child_idx, uint32_t child_page_id) {
    uint32_t z_page_id = bpm_.new_page();

    storage::SlottedPage* child_page = bpm_.fetch_page(child_page_id);
    std::vector<uint8_t> child_buf;
    child_page->get_record(0, child_buf);
    BTreeNodePayload y{};
    std::memcpy(&y, child_buf.data(), sizeof(BTreeNodePayload));

    BTreeNodePayload z{};
    z.is_leaf = y.is_leaf;

    constexpr size_t mid = MAX_BTREE_KEYS / 2;
    uint32_t promoted_key = 0;

    if (y.is_leaf) {
        // Keep the separator record in the right leaf (B+ tree semantics).
        z.num_keys = static_cast<uint16_t>(MAX_BTREE_KEYS - mid);
        for (size_t j = 0; j < z.num_keys; ++j) {
            z.keys[j] = y.keys[mid + j];
            z.record_ids[j] = y.record_ids[mid + j];
        }

        z.next_leaf_page_id = y.next_leaf_page_id;
        y.next_leaf_page_id = z_page_id;
        y.num_keys = static_cast<uint16_t>(mid);

        promoted_key = z.keys[0];
    } else {
        // Internal-node split: promote the median; it is not retained
        // in either child.
        promoted_key = y.keys[mid];
        z.num_keys = static_cast<uint16_t>(MAX_BTREE_KEYS - mid - 1);

        for (size_t j = 0; j < z.num_keys; ++j) {
            z.keys[j] = y.keys[mid + 1 + j];
        }
        for (size_t j = 0; j <= z.num_keys; ++j) {
            z.child_page_ids[j] = y.child_page_ids[mid + 1 + j];
        }

        y.num_keys = static_cast<uint16_t>(mid);
    }

    storage::SlottedPage* parent_page = bpm_.fetch_page(parent_page_id);
    std::vector<uint8_t> parent_buf;
    parent_page->get_record(0, parent_buf);
    BTreeNodePayload p{};
    std::memcpy(&p, parent_buf.data(), sizeof(BTreeNodePayload));

    for (int j = static_cast<int>(p.num_keys); j > child_idx; --j) {
        p.child_page_ids[j + 1] = p.child_page_ids[j];
    }
    p.child_page_ids[child_idx + 1] = z_page_id;

    for (int j = static_cast<int>(p.num_keys) - 1; j >= child_idx; --j) {
        p.keys[j + 1] = p.keys[j];
    }
    p.keys[child_idx] = promoted_key;
    ++p.num_keys;

    storage::SlottedPage new_p_page;
    new_p_page.insert_record(reinterpret_cast<const uint8_t*>(&p), sizeof(BTreeNodePayload));
    std::memcpy(parent_page->raw_data(), new_p_page.raw_data(), storage::PAGE_SIZE);

    storage::SlottedPage new_y_page;
    new_y_page.insert_record(reinterpret_cast<const uint8_t*>(&y), sizeof(BTreeNodePayload));
    std::memcpy(child_page->raw_data(), new_y_page.raw_data(), storage::PAGE_SIZE);

    storage::SlottedPage* z_page = bpm_.fetch_page(z_page_id);
    z_page->insert_record(reinterpret_cast<const uint8_t*>(&z), sizeof(BTreeNodePayload));

    bpm_.unpin_page(parent_page_id, true);
    bpm_.unpin_page(child_page_id, true);
    bpm_.unpin_page(z_page_id, true);
}

void BTreeIndex::insert_non_full(uint32_t node_page_id, uint32_t key, RecordID rid) {
    storage::SlottedPage* page = bpm_.fetch_page(node_page_id);
    std::vector<uint8_t> buf;
    page->get_record(0, buf);
    BTreeNodePayload node;
    std::memcpy(&node, buf.data(), sizeof(BTreeNodePayload));

    int i = node.num_keys - 1;

    if (node.is_leaf) {
        while (i >= 0 && key < node.keys[i]) {
            node.keys[i + 1] = node.keys[i];
            node.record_ids[i + 1] = node.record_ids[i];
            i--;
        }
        node.keys[i + 1] = key;
        node.record_ids[i + 1] = rid;
        node.num_keys++;

        storage::SlottedPage updated_page;
        updated_page.insert_record(reinterpret_cast<const uint8_t*>(&node), sizeof(BTreeNodePayload));
        std::memcpy(page->raw_data(), updated_page.raw_data(), storage::PAGE_SIZE);
        bpm_.unpin_page(node_page_id, true);
    } else {
        while (i >= 0 && key < node.keys[i]) {
            i--;
        }
        i++;
        uint32_t child_page_id = node.child_page_ids[i];

        storage::SlottedPage* child_page = bpm_.fetch_page(child_page_id);
        std::vector<uint8_t> child_buf;
        child_page->get_record(0, child_buf);
        BTreeNodePayload child;
        std::memcpy(&child, child_buf.data(), sizeof(BTreeNodePayload));

        if (child.num_keys == MAX_BTREE_KEYS) {
            split_child(node_page_id, i, child_page_id);
            // Reload node after split
            page = bpm_.fetch_page(node_page_id);
            page->get_record(0, buf);
            std::memcpy(&node, buf.data(), sizeof(BTreeNodePayload));

            if (key > node.keys[i]) {
                i++;
            }
        }
        insert_non_full(node.child_page_ids[i], key, rid);
    }
}

} // namespace vdb_index