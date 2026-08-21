#include <gtest/gtest.h>
#include <cstdio>
#include <string>
#include <vector>

#include "storage/buffer_pool.h"

namespace {

class BufferPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = "test_buffer_pool_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + ".db";
        std::remove(db_path_.c_str());
    }

    void TearDown() override {
        std::remove(db_path_.c_str());
    }

    std::string db_path_;
};

TEST_F(BufferPoolTest, NewPageAllocatesSequentialIds) {
    storage::BufferPoolManager bpm(db_path_, 10);
    uint32_t p0 = bpm.new_page();
    uint32_t p1 = bpm.new_page();
    uint32_t p2 = bpm.new_page();
    EXPECT_EQ(p0, 0u);
    EXPECT_EQ(p1, 1u);
    EXPECT_EQ(p2, 2u);
}

TEST_F(BufferPoolTest, WriteThenFetchReturnsSameData) {
    storage::BufferPoolManager bpm(db_path_, 10);
    uint32_t page_id = bpm.new_page();

    storage::SlottedPage* page = bpm.fetch_page(page_id);
    std::vector<uint8_t> payload = {10, 20, 30, 40};
    int32_t slot = page->insert_record(payload.data(), static_cast<uint16_t>(payload.size()));
    ASSERT_GE(slot, 0);
    bpm.unpin_page(page_id, true);

    storage::SlottedPage* refetched = bpm.fetch_page(page_id);
    std::vector<uint8_t> out;
    ASSERT_TRUE(refetched->get_record(static_cast<uint16_t>(slot), out));
    EXPECT_EQ(out, payload);
}

TEST_F(BufferPoolTest, DataSurvivesFlushAndReopenFromDisk) {
    uint32_t page_id;
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5, 6, 7, 8};

    {
        storage::BufferPoolManager bpm(db_path_, 10);
        page_id = bpm.new_page();
        storage::SlottedPage* page = bpm.fetch_page(page_id);
        page->insert_record(payload.data(), static_cast<uint16_t>(payload.size()));
        bpm.unpin_page(page_id, true);
        bpm.flush_page(page_id);
        // bpm destructs here, closing the file handle.
    }

    {
        // Fresh manager over the same file: page must be readable from disk.
        storage::BufferPoolManager bpm(db_path_, 10);
        storage::SlottedPage* page = bpm.fetch_page(page_id);
        std::vector<uint8_t> out;
        ASSERT_TRUE(page->get_record(0, out));
        EXPECT_EQ(out, payload);
    }
}

TEST_F(BufferPoolTest, FlushNonexistentPageReturnsFalse) {
    storage::BufferPoolManager bpm(db_path_, 10);
    EXPECT_FALSE(bpm.flush_page(9999));
}

}  // namespace
