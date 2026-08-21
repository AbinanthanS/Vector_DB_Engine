#include "query/engine.h"
#include <iostream>
#include <stdexcept>

namespace query {

ExecutionEngine::ExecutionEngine(storage::BufferPoolManager& bpm, size_t dim)
    : bpm_(bpm), dim_(dim), btree_(bpm), hnsw_(bpm, dim) {}

vdb_index::RecordID ExecutionEngine::insert_record(uint32_t id, uint32_t category, const std::vector<float>& vec) {
    // Pack multiple vectors into 4KB slotted pages instead of allocating one
    // physical page per vector.
    constexpr uint32_t INVALID_PAGE = 0xFFFFFFFF;
    const uint16_t bytes = static_cast<uint16_t>(vec.size() * sizeof(float));

    uint32_t page_id = current_vector_page_id_;
    int32_t slot_id = -1;

    if (page_id != INVALID_PAGE) {
        storage::SlottedPage* page = bpm_.fetch_page(page_id);
        slot_id = page->insert_record(
            reinterpret_cast<const uint8_t*>(vec.data()), bytes);
        if (slot_id >= 0) {
            bpm_.unpin_page(page_id, true);
        }
    }

    if (slot_id < 0) {
        page_id = bpm_.new_page();
        current_vector_page_id_ = page_id;

        storage::SlottedPage* page = bpm_.fetch_page(page_id);
        slot_id = page->insert_record(
            reinterpret_cast<const uint8_t*>(vec.data()), bytes);
        if (slot_id < 0) {
            bpm_.unpin_page(page_id, false);
            throw std::runtime_error("Vector does not fit in a fresh page.");
        }
        bpm_.unpin_page(page_id, true);
    }

    vdb_index::RecordID rid{page_id, static_cast<uint16_t>(slot_id)};

    // 2. Index scalar metadata in B+ Tree.
    btree_.insert(category, rid);

    // 3. Index vector in HNSW graph/cache.
    hnsw_.insert(id, vec, rid);
    node_category_map_[id] = category;

    return rid;
}

std::vector<std::pair<float, vdb_index::RecordID>> ExecutionEngine::hybrid_query(
    uint32_t filter_category,
    const std::vector<float>& query_vec,
    size_t k
) {
    std::vector<vdb_index::RecordID> valid_rids;
    if (!btree_.search(filter_category, valid_rids)) {
        return {};
    }

    // Exact filtered search: the B+ tree supplies the candidate posting list,
    // then AVX2/FMA distance computation evaluates only those candidates.
    return hnsw_.search_filtered(query_vec, valid_rids, k);
}

} // namespace query