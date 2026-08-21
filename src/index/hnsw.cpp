#include "index/hnsw.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace vdb_index {

HNSWIndex::HNSWIndex(storage::BufferPoolManager& bpm, size_t dim, size_t ef_construction)
    : bpm_(bpm), dim_(dim), ef_construction_(ef_construction), entry_node_id_(0xFFFFFFFF), max_level_(-1), rng_(42) {}

uint64_t HNSWIndex::rid_key(RecordID rid) {
    return (static_cast<uint64_t>(rid.page_id) << 16) | rid.slot_id;
}

int HNSWIndex::generate_random_level() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double r = dist(rng_);
    if (r == 0.0) r = 0.0000001;
    int level = static_cast<int>(-std::log(r) * 0.5);
    return std::min(level, static_cast<int>(MAX_LAYERS - 1));
}

float HNSWIndex::get_distance(const std::vector<float>& a, uint32_t node_b_id) {
    const float* vec_b = vector_cache_.data() + static_cast<size_t>(node_b_id) * dim_;
    return math::calculate_cosine_distance(a.data(), vec_b, dim_);
}

void HNSWIndex::insert(uint32_t node_id, const std::vector<float>& vec, RecordID rid) {
    const size_t required = (static_cast<size_t>(node_id) + 1) * dim_;
    if (vector_cache_.size() < required) vector_cache_.resize(required);
    std::copy(vec.begin(), vec.end(),
              vector_cache_.begin() + static_cast<size_t>(node_id) * dim_);
    rid_to_node_[rid_key(rid)] = node_id;

    HNSWNodePayload new_node{};
    new_node.node_id = node_id;
    new_node.record_id = rid;
    for (size_t l = 0; l < MAX_LAYERS; ++l) {
        new_node.num_neighbors[l] = 0;
    }

    int insert_level = generate_random_level();

    if (entry_node_id_ == 0xFFFFFFFF) { // First node inserted in graph
        entry_node_id_ = node_id;
        max_level_ = insert_level;
        node_cache_[node_id] = new_node;
        return;
    }

    uint32_t curr_obj = entry_node_id_;

    // Greedy search down to insertion level
    for (int level = max_level_; level > insert_level; --level) {
        bool changed = true;
        while (changed) {
            changed = false;
            float curr_dist = get_distance(vec, curr_obj);
            
            const auto& neighbors = node_cache_[curr_obj];
            for (uint16_t i = 0; i < neighbors.num_neighbors[level]; ++i) {
                uint32_t neighbor_id = neighbors.neighbors[level][i];
                float d = get_distance(vec, neighbor_id);
                if (d < curr_dist) {
                    curr_dist = d;
                    curr_obj = neighbor_id;
                    changed = true;
                }
            }
        }
    }

    // Connect neighbors from insert_level down to level 0
    for (int level = std::min(insert_level, max_level_); level >= 0; --level) {
        auto& entry = node_cache_[curr_obj];
        if (entry.num_neighbors[level] < MAX_NEIGHBORS) {
            entry.neighbors[level][entry.num_neighbors[level]++] = node_id;
            new_node.neighbors[level][new_node.num_neighbors[level]++] = curr_obj;
        }
    }

    node_cache_[node_id] = new_node;

    if (insert_level > max_level_) {
        max_level_ = insert_level;
        entry_node_id_ = node_id;
    }
}

std::vector<std::pair<float, RecordID>> HNSWIndex::search(const std::vector<float>& query_vec, size_t k) {
    std::vector<std::pair<float, RecordID>> results;
    if (entry_node_id_ == 0xFFFFFFFF) return results;

    uint32_t curr_obj = entry_node_id_;
    float curr_dist = get_distance(query_vec, curr_obj);

    // Greedy navigation to layer 0
    for (int level = max_level_; level > 0; --level) {
        bool changed = true;
        while (changed) {
            changed = false;
            const auto& node = node_cache_[curr_obj];
            for (uint16_t i = 0; i < node.num_neighbors[level]; ++i) {
                uint32_t neighbor_id = node.neighbors[level][i];
                float d = get_distance(query_vec, neighbor_id);
                if (d < curr_dist) {
                    curr_dist = d;
                    curr_obj = neighbor_id;
                    changed = true;
                }
            }
        }
    }

    // Best-first search on Layer 0
    using DistPair = std::pair<float, uint32_t>;
    std::priority_queue<DistPair, std::vector<DistPair>, std::greater<DistPair>> visited_queue;
    std::unordered_set<uint32_t> visited;

    visited_queue.push({curr_dist, curr_obj});
    visited.insert(curr_obj);

    while (!visited_queue.empty() && results.size() < k) {
        auto [dist, node_id] = visited_queue.top();
        visited_queue.pop();

        results.push_back({dist, node_cache_[node_id].record_id});

        const auto& node = node_cache_[node_id];
        for (uint16_t i = 0; i < node.num_neighbors[0]; ++i) {
            uint32_t neighbor_id = node.neighbors[0][i];
            if (visited.find(neighbor_id) == visited.end()) {
                visited.insert(neighbor_id);
                float d = get_distance(query_vec, neighbor_id);
                visited_queue.push({d, neighbor_id});
            }
        }
    }

    return results;
}


std::vector<std::pair<float, RecordID>> HNSWIndex::search_filtered(
    const std::vector<float>& query_vec,
    const std::vector<RecordID>& valid_rids,
    size_t k
) {
    if (k == 0 || valid_rids.empty()) return {};

    // Keep only the best k distances. This avoids sorting the entire
    // filtered candidate set.
    struct Candidate {
        float distance;
        RecordID rid;
    };
    struct WorseFirst {
        bool operator()(const Candidate& a, const Candidate& b) const {
            return a.distance < b.distance; // max-heap: worst distance at top
        }
    };

    std::priority_queue<Candidate, std::vector<Candidate>, WorseFirst> topk;

    for (const RecordID& rid : valid_rids) {
        auto it = rid_to_node_.find(rid_key(rid));
        if (it == rid_to_node_.end()) continue;

        uint32_t node_id = it->second;
        float d = get_distance(query_vec, node_id);

        if (topk.size() < k) {
            topk.push({d, rid});
        } else if (d < topk.top().distance) {
            topk.pop();
            topk.push({d, rid});
        }
    }

    std::vector<std::pair<float, RecordID>> results;
    results.reserve(topk.size());
    while (!topk.empty()) {
        results.push_back({topk.top().distance, topk.top().rid});
        topk.pop();
    }
    std::sort(results.begin(), results.end(),
              [](const auto& x, const auto& y) { return x.first < y.first; });
    return results;
}


} // namespace vdb_index
