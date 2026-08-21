#include <iostream>
#include <vector>
#include "storage/buffer_pool.h"
#include "index/hnsw.h"

int main() {
    const std::string db_file = "hnsw_test.db";
    storage::BufferPoolManager bpm(db_file, 10);

    constexpr size_t DIM = 128;
    index::HNSWIndex hnsw(bpm, DIM);

    // Insert 3 target vectors
    std::vector<float> vec0(DIM, 0.1f); // Node 0
    std::vector<float> vec1(DIM, 0.5f); // Node 1 (Closest to query)
    std::vector<float> vec2(DIM, 0.9f); // Node 2

    index::RecordID rid0{1, 0};
    index::RecordID rid1{1, 1};
    index::RecordID rid2{1, 2};

    hnsw.insert(0, vec0, rid0);
    hnsw.insert(1, vec1, rid1);
    hnsw.insert(2, vec2, rid2);

    std::cout << "[HNSW] Inserted 3 vectors (128-dim) into spatial graph." << std::endl;

    // Search query vector closest to vec1 (0.5f)
    std::vector<float> query_vec(DIM, 0.52f);
    auto results = hnsw.search(query_vec, 2); // Retrieve Top-2 nearest neighbors

    std::cout << "[HNSW Search] Top-2 Nearest Neighbors for Query:" << std::endl;
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << " Rank " << i + 1 << " -> Cosine Distance: " << results[i].first
                  << " | Page ID: " << results[i].second.page_id 
                  << ", Slot ID: " << results[i].second.slot_id << std::endl;
    }

    return 0;
}