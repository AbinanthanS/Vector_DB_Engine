#ifndef HNSW_H
#define HNSW_H

#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <random>
#include "storage/buffer_pool.h"
#include "math/distance.h"
#include "index/btree.h"

namespace index {

constexpr size_t MAX_NEIGHBORS = 16;
constexpr size_t MAX_LAYERS = 4;

struct HNSWNodePayload {
    uint32_t node_id;
    RecordID record_id; // Pointer to physical vector record on 4KB page
    uint16_t num_neighbors[MAX_LAYERS];
    uint32_t neighbors[MAX_LAYERS][MAX_NEIGHBORS]; // Adjacency list using Node IDs
};

class HNSWIndex {
public:
    HNSWIndex(storage::BufferPoolManager& bpm, size_t dim, size_t ef_construction = 64);

    // Insert a vector and map it to a physical page RecordID
    void insert(uint32_t node_id, const std::vector<float>& vec, RecordID rid);

    // Search for Top-K Nearest Neighbors
    std::vector<std::pair<float, RecordID>> search(const std::vector<float>& query_vec, size_t k);

private:
    storage::BufferPoolManager& bpm_;
    size_t dim_;
    size_t ef_construction_;
    uint32_t entry_node_id_;
    int max_level_;

    std::mt19937 rng_;
    std::unordered_map<uint32_t, std::vector<float>> vector_cache_;
    std::unordered_map<uint32_t, HNSWNodePayload> node_cache_;

    int generate_random_level();
    float get_distance(const std::vector<float>& a, uint32_t node_b_id);
};

} // namespace index

#endif // HNSW_H