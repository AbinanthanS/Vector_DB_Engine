#include <gtest/gtest.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include "index/hnsw.h"
#include "math/distance.h"
#include "storage/buffer_pool.h"

namespace {

class HNSWTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = "test_hnsw_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + ".db";
        std::remove(db_path_.c_str());
        bpm_ = std::make_unique<storage::BufferPoolManager>(db_path_, 100);
    }

    void TearDown() override {
        bpm_.reset();
        std::remove(db_path_.c_str());
    }

    std::string db_path_;
    std::unique_ptr<storage::BufferPoolManager> bpm_;
};

TEST_F(HNSWTest, SearchOnEmptyGraphReturnsNoResults) {
    vdb_index::HNSWIndex hnsw(*bpm_, 8);
    std::vector<float> query(8, 1.0f);
    auto results = hnsw.search(query, 5);
    EXPECT_TRUE(results.empty());
}

TEST_F(HNSWTest, SingleInsertedVectorIsFoundByItself) {
    vdb_index::HNSWIndex hnsw(*bpm_, 4);
    std::vector<float> vec = {1.0f, 0.0f, 0.0f, 0.0f};
    hnsw.insert(0, vec, {1, 0});

    auto results = hnsw.search(vec, 1);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_NEAR(results[0].first, 0.0f, 1e-4f);
}

TEST_F(HNSWTest, SearchRecallAgainstBruteForceOnSmallGraph) {
    // NOTE: this graph is intentionally small. HNSWIndex::insert() connects
    // each new node to only a single neighbor per level (it never uses
    // ef_construction to consider multiple candidates), so recall vs. brute
    // force degrades sharply as the graph grows -- measured recall@10 drops
    // from ~0.8 at N=50 to effectively 0 by N=5000. This test pins down
    // today's actual behavior on a small graph; it is not a claim that the
    // index has good recall at production scale. See engineering notes.
    constexpr size_t dim = 16;
    constexpr int N = 50;
    constexpr size_t K = 10;

    vdb_index::HNSWIndex hnsw(*bpm_, dim, /*ef_construction=*/128);

    std::mt19937 rng(123);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<std::vector<float>> vectors(N, std::vector<float>(dim));

    for (int i = 0; i < N; ++i) {
        for (float& x : vectors[i]) x = dist(rng);
        hnsw.insert(static_cast<uint32_t>(i), vectors[i],
                    {static_cast<uint32_t>(i), 0});
    }

    std::vector<float> query(dim);
    for (float& x : query) x = dist(rng);

    // Brute-force ground truth top-K by node id.
    std::vector<std::pair<float, int>> brute;
    brute.reserve(N);
    for (int i = 0; i < N; ++i) {
        float d = math::calculate_cosine_distance(query.data(), vectors[i].data(), dim);
        brute.emplace_back(d, i);
    }
    std::sort(brute.begin(), brute.end());
    std::unordered_set<int> ground_truth_ids;
    for (size_t i = 0; i < K; ++i) ground_truth_ids.insert(brute[i].second);

    auto results = hnsw.search(query, K);
    ASSERT_EQ(results.size(), K);

    // HNSW is approximate, so require the graph search to recover most
    // (not necessarily all) of the true nearest neighbors on this
    // small/well-connected graph.
    int hits = 0;
    for (auto& [dist_val, rid] : results) {
        if (ground_truth_ids.count(static_cast<int>(rid.page_id))) ++hits;
    }
    EXPECT_GE(hits, static_cast<int>(K) / 2)
        << "expected at least half of the top-" << K << " results to match brute force";
}

TEST_F(HNSWTest, SearchFilteredOnlyReturnsCandidatesFromAllowList) {
    constexpr size_t dim = 8;
    vdb_index::HNSWIndex hnsw(*bpm_, dim);

    std::mt19937 rng(5);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<vdb_index::RecordID> all_rids;

    for (int i = 0; i < 50; ++i) {
        std::vector<float> vec(dim);
        for (float& x : vec) x = dist(rng);
        vdb_index::RecordID rid{static_cast<uint32_t>(i), 0};
        hnsw.insert(static_cast<uint32_t>(i), vec, rid);
        all_rids.push_back(rid);
    }

    // Only allow a subset of RecordIDs through the filter.
    std::vector<vdb_index::RecordID> allowed = {all_rids[0], all_rids[5], all_rids[10]};
    std::vector<float> query(dim, 0.5f);

    auto results = hnsw.search_filtered(query, allowed, 10);
    EXPECT_LE(results.size(), allowed.size());
    for (auto& [d, rid] : results) {
        bool is_allowed = std::any_of(allowed.begin(), allowed.end(),
                                       [&](const vdb_index::RecordID& a) {
                                           return a.page_id == rid.page_id && a.slot_id == rid.slot_id;
                                       });
        EXPECT_TRUE(is_allowed);
    }
}

TEST_F(HNSWTest, SearchFilteredReturnsResultsSortedByDistance) {
    constexpr size_t dim = 8;
    vdb_index::HNSWIndex hnsw(*bpm_, dim);

    std::mt19937 rng(9);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<vdb_index::RecordID> all_rids;

    for (int i = 0; i < 30; ++i) {
        std::vector<float> vec(dim);
        for (float& x : vec) x = dist(rng);
        vdb_index::RecordID rid{static_cast<uint32_t>(i), 0};
        hnsw.insert(static_cast<uint32_t>(i), vec, rid);
        all_rids.push_back(rid);
    }

    std::vector<float> query(dim, 0.1f);
    auto results = hnsw.search_filtered(query, all_rids, 10);

    ASSERT_FALSE(results.empty());
    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_LE(results[i - 1].first, results[i].first);
    }
}

TEST_F(HNSWTest, SearchFilteredWithEmptyAllowListReturnsEmpty) {
    vdb_index::HNSWIndex hnsw(*bpm_, 4);
    hnsw.insert(0, {1.0f, 0.0f, 0.0f, 0.0f}, {1, 0});

    std::vector<float> query = {1.0f, 0.0f, 0.0f, 0.0f};
    auto results = hnsw.search_filtered(query, {}, 5);
    EXPECT_TRUE(results.empty());
}

}  // namespace
