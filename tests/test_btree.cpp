#include <gtest/gtest.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "index/btree.h"
#include "storage/buffer_pool.h"

namespace {

class BTreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = "test_btree_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + ".db";
        std::remove(db_path_.c_str());
        bpm_ = std::make_unique<storage::BufferPoolManager>(db_path_, 100);
        btree_ = std::make_unique<vdb_index::BTreeIndex>(*bpm_);
    }

    void TearDown() override {
        btree_.reset();
        bpm_.reset();
        std::remove(db_path_.c_str());
    }

    std::string db_path_;
    std::unique_ptr<storage::BufferPoolManager> bpm_;
    std::unique_ptr<vdb_index::BTreeIndex> btree_;
};

TEST_F(BTreeTest, SearchOnEmptyTreeFindsNothing) {
    std::vector<vdb_index::RecordID> out;
    EXPECT_FALSE(btree_->search(42, out));
    EXPECT_TRUE(out.empty());
}

TEST_F(BTreeTest, InsertThenSearchFindsExactMatch) {
    vdb_index::RecordID rid{7, 3};
    btree_->insert(100, rid);

    std::vector<vdb_index::RecordID> out;
    ASSERT_TRUE(btree_->search(100, out));
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], rid);
}

TEST_F(BTreeTest, SearchForMissingKeyReturnsFalse) {
    btree_->insert(5, {1, 1});
    btree_->insert(10, {2, 2});

    std::vector<vdb_index::RecordID> out;
    EXPECT_FALSE(btree_->search(7, out));
}

TEST_F(BTreeTest, DuplicateKeysReturnAllMatchingRecords) {
    btree_->insert(50, {1, 0});
    btree_->insert(50, {2, 0});
    btree_->insert(50, {3, 0});

    std::vector<vdb_index::RecordID> out;
    ASSERT_TRUE(btree_->search(50, out));
    EXPECT_EQ(out.size(), 3u);
}

TEST_F(BTreeTest, SurvivesEnoughInsertsToForceNodeSplits) {
    // MAX_BTREE_KEYS == 63, so this forces multiple splits and a taller tree.
    constexpr int N = 5000;
    for (int i = 0; i < N; ++i) {
        btree_->insert(static_cast<uint32_t>(i), {static_cast<uint32_t>(i), 0});
    }

    for (int i = 0; i < N; ++i) {
        std::vector<vdb_index::RecordID> out;
        ASSERT_TRUE(btree_->search(static_cast<uint32_t>(i), out)) << "missing key " << i;
        EXPECT_EQ(out[0].page_id, static_cast<uint32_t>(i));
    }
}

TEST_F(BTreeTest, HandlesRandomInsertionOrderWithDuplicates) {
    constexpr int N = 3000;
    std::vector<int> keys(N);
    for (int i = 0; i < N; ++i) keys[i] = i;
    std::mt19937 rng(42);
    std::shuffle(keys.begin(), keys.end(), rng);

    for (int k : keys) {
        btree_->insert(static_cast<uint32_t>(k), {static_cast<uint32_t>(k), 0});
        btree_->insert(static_cast<uint32_t>(k), {static_cast<uint32_t>(k), 1});
    }

    for (int k : keys) {
        std::vector<vdb_index::RecordID> out;
        ASSERT_TRUE(btree_->search(static_cast<uint32_t>(k), out));
        EXPECT_EQ(out.size(), 2u);
    }

    // Keys that were never inserted must not be found.
    for (int k = N; k < N + 100; ++k) {
        std::vector<vdb_index::RecordID> out;
        EXPECT_FALSE(btree_->search(static_cast<uint32_t>(k), out));
    }
}

TEST_F(BTreeTest, BoundaryKeysAtSplitPointsAreFindable) {
    // Insert keys that are highly likely to land exactly on separator
    // boundaries after repeated splits (regression check for the
    // internal-node descent comparison).
    constexpr int N = 2000;
    for (int i = 0; i < N; ++i) {
        btree_->insert(static_cast<uint32_t>(i * 2), {static_cast<uint32_t>(i), 0});
    }

    for (int i = 0; i < N; ++i) {
        std::vector<vdb_index::RecordID> out;
        ASSERT_TRUE(btree_->search(static_cast<uint32_t>(i * 2), out)) << "missing key " << (i * 2);
    }
}

}  // namespace
