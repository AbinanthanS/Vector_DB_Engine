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

namespace vdb_index {

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
    // Exact Top-K over a pre-filtered RecordID posting list. This is the
    // correctness-preserving hybrid-search path.
    std::vector<std::pair<float, RecordID>> search_filtered(
        const std::vector<float>& query_vec,
        const std::vector<RecordID>& valid_rids,
        size_t k
    );

private:
    storage::BufferPoolManager& bpm_;
    size_t dim_;
    size_t ef_construction_;
    uint32_t entry_node_id_;
    int max_level_;

    std::mt19937 rng_;
    std::vector<float> vector_cache_; // contiguous FP32 storage: node_id * dim_
    std::unordered_map<uint32_t, HNSWNodePayload> node_cache_;
    std::unordered_map<uint64_t, uint32_t> rid_to_node_;

    int generate_random_level();
    float get_distance(const std::vector<float>& a, uint32_t node_b_id);
    float get_distance_nodes(uint32_t node_a_id, uint32_t node_b_id);
    std::vector<std::pair<float, uint32_t>> search_layer(
        const std::vector<float>& query, uint32_t entry_point, size_t ef, int level);
    static uint64_t rid_key(RecordID rid);
};

} // namespace vdb_index

#endif // HNSW_H