#include <iostream>
#include <vector>
#include "storage/buffer_pool.h"
#include "query/engine.h"

int main() {
    const std::string db_file = "engine_hybrid_test.db";
    storage::BufferPoolManager bpm(db_file, 10);

    constexpr size_t DIM = 128;
    query::ExecutionEngine engine(bpm, DIM);

    // Setup 3 vectors across 2 distinct categories (Category 10 = 'Tech', Category 20 = 'Finance')
    std::vector<float> vec0(DIM, 0.1f); // Tech
    std::vector<float> vec1(DIM, 0.5f); // Finance (Closest vector match!)
    std::vector<float> vec2(DIM, 0.9f); // Tech

    engine.insert_record(0, 10, vec0); // ID: 0, Category: 10
    engine.insert_record(1, 20, vec1); // ID: 1, Category: 20
    engine.insert_record(2, 10, vec2); // ID: 2, Category: 10

    std::cout << "[Execution Engine] Inserted records across scalar categories 10 (Tech) & 20 (Finance)." << std::endl;

    // Search query vector (0.52f) with a strict scalar category pre-filter = 10 ('Tech')
    // Note: vec1 (0.5f) is closest in distance, but SHOULD BE PRUNED because its category is 20!
    std::vector<float> query_vec(DIM, 0.52f);
    uint32_t filter_category = 10;

    std::cout << "\n[Hybrid Query] Executing Search for Query Vector (0.52f) WHERE Category == 10..." << std::endl;
    auto results = engine.hybrid_query(filter_category, query_vec, 2);

    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << " Result Rank " << i + 1 << " -> Cosine Distance: " << results[i].first
                  << " | Physical Page ID: " << results[i].second.page_id 
                  << ", Slot ID: " << results[i].second.slot_id << std::endl;
    }

    return 0;
}