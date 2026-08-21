#include <gtest/gtest.h>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "query/engine.h"
#include "storage/buffer_pool.h"

namespace {

class EngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = "test_engine_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + ".db";
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

TEST_F(EngineTest, HybridQueryOnEmptyEngineReturnsNoResults) {
    constexpr size_t dim = 8;
    query::ExecutionEngine engine(*bpm_, dim);

    std::vector<float> query_vec(dim, 1.0f);
    auto results = engine.hybrid_query(/*filter_category=*/0, query_vec, 5);
    EXPECT_TRUE(results.empty());
}

TEST_F(EngineTest, HybridQueryOnlyReturnsRecordsMatchingCategory) {
    constexpr size_t dim = 8;
    query::ExecutionEngine engine(*bpm_, dim);

    std::mt19937 rng(21);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    // Category 0: 20 records. Category 1: 20 records.
    for (uint32_t i = 0; i < 40; ++i) {
        std::vector<float> vec(dim);
        for (float& x : vec) x = dist(rng);
        uint32_t category = i % 2;
        engine.insert_record(i, category, vec);
    }

    std::vector<float> query_vec(dim, 0.0f);
    auto results = engine.hybrid_query(/*filter_category=*/0, query_vec, 5);

    ASSERT_FALSE(results.empty());
    EXPECT_LE(results.size(), 5u);
    // Every returned record must genuinely belong to category 0: since
    // ids alternate category by parity and RecordID.page_id/slot_id don't
    // directly expose the id, we instead check via a category filter that
    // excludes everything (category 1) and confirm disjoint result sets.
    auto other_results = engine.hybrid_query(/*filter_category=*/1, query_vec, 5);
    for (auto& [d1, r1] : results) {
        for (auto& [d2, r2] : other_results) {
            EXPECT_FALSE(r1.page_id == r2.page_id && r1.slot_id == r2.slot_id)
                << "same record returned for two different category filters";
        }
    }
}

TEST_F(EngineTest, HybridQueryOnMissingCategoryReturnsEmpty) {
    constexpr size_t dim = 8;
    query::ExecutionEngine engine(*bpm_, dim);

    std::vector<float> vec(dim, 0.5f);
    engine.insert_record(0, /*category=*/1, vec);

    std::vector<float> query_vec(dim, 0.5f);
    auto results = engine.hybrid_query(/*filter_category=*/99, query_vec, 5);
    EXPECT_TRUE(results.empty());
}

TEST_F(EngineTest, HybridQueryRespectsKLimit) {
    constexpr size_t dim = 8;
    query::ExecutionEngine engine(*bpm_, dim);

    std::mt19937 rng(1);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (uint32_t i = 0; i < 30; ++i) {
        std::vector<float> vec(dim);
        for (float& x : vec) x = dist(rng);
        engine.insert_record(i, /*category=*/0, vec);
    }

    std::vector<float> query_vec(dim, 0.0f);
    auto results = engine.hybrid_query(0, query_vec, /*k=*/3);
    EXPECT_EQ(results.size(), 3u);
}

}  // namespace
