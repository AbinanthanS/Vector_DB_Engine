#ifndef ENGINE_H
#define ENGINE_H

#include <vector>
#include <utility>
#include "storage/buffer_pool.h"
#include "index/btree.h"
#include "index/hnsw.h"

namespace query {

struct RecordEntry {
    uint32_t record_id_num;
    uint32_t scalar_category;
    std::vector<float> vector_data;
};

class ExecutionEngine {
public:
    ExecutionEngine(storage::BufferPoolManager& bpm, size_t dim);

    // Insert record atomically into storage, B+ Tree index, and HNSW graph
    index::RecordID insert_record(uint32_t id, uint32_t category, const std::vector<float>& vec);

    // Execute single-stage hybrid search (Filter scalar category AND retrieve Top-K vectors)
    std::vector<std::pair<float, index::RecordID>> hybrid_query(
        uint32_t filter_category, 
        const std::vector<float>& query_vec, 
        size_t k
    );

private:
    storage::BufferPoolManager& bpm_;
    size_t dim_;
    index::BTreeIndex btree_;
    index::HNSWIndex hnsw_;
    
    // Tracks category mapping per inserted node for single-stage evaluation
    std::unordered_map<uint32_t, uint32_t> node_category_map_; 
};

} // namespace query

#endif // ENGINE_H