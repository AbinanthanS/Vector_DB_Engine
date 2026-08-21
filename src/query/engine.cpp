#include "query/engine.h"
#include <iostream>

namespace query {

ExecutionEngine::ExecutionEngine(storage::BufferPoolManager& bpm, size_t dim)
    : bpm_(bpm), dim_(dim), btree_(bpm), hnsw_(bpm, dim) {}

index::RecordID ExecutionEngine::insert_record(uint32_t id, uint32_t category, const std::vector<float>& vec) {
    // 1. Write vector to 4KB Slotted-Page in Storage Layer
    uint32_t page_id = bpm_.new_page();
    storage::SlottedPage* page = bpm_.fetch_page(page_id);
    
    uint16_t bytes = vec.size() * sizeof(float);
    int32_t slot_id = page->insert_record(reinterpret_cast<const uint8_t*>(vec.data()), bytes);
    
    bpm_.unpin_page(page_id, true);
    bpm_.flush_page(page_id);

    index::RecordID rid{page_id, static_cast<uint16_t>(slot_id)};

    // 2. Index scalar metadata in B+ Tree
    btree_.insert(category, rid);

    // 3. Index vector in HNSW Graph
    hnsw_.insert(id, vec, rid);
    node_category_map_[id] = category;

    return rid;
}

std::vector<std::pair<float, index::RecordID>> ExecutionEngine::hybrid_query(
    uint32_t filter_category, 
    const std::vector<float>& query_vec, 
    size_t k
) {
    // Single-Stage Filter: Evaluate scalar validity from B+ Tree lookup
    std::vector<index::RecordID> valid_rids;
    bool category_exists = btree_.search(filter_category, valid_rids);

    if (!category_exists) {
        std::cout << "[Engine] Category filter matches 0 records. Pruning vector search completely." << std::endl;
        return {};
    }

    // Retrieve nearest neighbors via HNSW graph traversal
    auto candidates = hnsw_.search(query_vec, k * 2); // Over-fetch candidates for filtered space

    std::vector<std::pair<float, index::RecordID>> filtered_results;
    for (const auto& candidate : candidates) {
        // Evaluate pre-filter condition
        for (const auto& valid_rid : valid_rids) {
            if (candidate.second == valid_rid) {
                filtered_results.push_back(candidate);
                break;
            }
        }
        if (filtered_results.size() == k) break;
    }

    return filtered_results;
}

} // namespace query