#include <gtest/gtest.h>
#include <vector>

#include "storage/page.h"

namespace {

TEST(SlottedPage, StartsEmptyWithFullFreeSpace) {
    storage::SlottedPage page;
    EXPECT_EQ(page.get_free_space(), storage::PAGE_SIZE - sizeof(storage::PageHeader));
}

TEST(SlottedPage, InsertAndRetrieveSingleRecord) {
    storage::SlottedPage page;
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};

    int32_t slot = page.insert_record(payload.data(), static_cast<uint16_t>(payload.size()));
    ASSERT_GE(slot, 0);

    std::vector<uint8_t> out;
    ASSERT_TRUE(page.get_record(static_cast<uint16_t>(slot), out));
    EXPECT_EQ(out, payload);
}

TEST(SlottedPage, InsertMultipleRecordsPreservesEachPayload) {
    storage::SlottedPage page;
    std::vector<std::vector<uint8_t>> payloads = {
        {1, 2, 3},
        {9, 9, 9, 9, 9},
        {42},
    };

    std::vector<int32_t> slots;
    for (auto& p : payloads) {
        int32_t slot = page.insert_record(p.data(), static_cast<uint16_t>(p.size()));
        ASSERT_GE(slot, 0);
        slots.push_back(slot);
    }

    for (size_t i = 0; i < payloads.size(); ++i) {
        std::vector<uint8_t> out;
        ASSERT_TRUE(page.get_record(static_cast<uint16_t>(slots[i]), out));
        EXPECT_EQ(out, payloads[i]);
    }
}

TEST(SlottedPage, GetRecordFailsForOutOfRangeSlot) {
    storage::SlottedPage page;
    std::vector<uint8_t> out;
    EXPECT_FALSE(page.get_record(0, out));  // no records inserted yet
    EXPECT_FALSE(page.get_record(999, out));
}

TEST(SlottedPage, InsertFailsWhenPageIsFull) {
    storage::SlottedPage page;
    std::vector<uint8_t> big(storage::PAGE_SIZE, 0xAB);

    int32_t slot = page.insert_record(big.data(), static_cast<uint16_t>(big.size()));
    EXPECT_EQ(slot, -1);
}

TEST(SlottedPage, FreeSpaceShrinksAsRecordsAreInserted) {
    storage::SlottedPage page;
    size_t before = page.get_free_space();

    std::vector<uint8_t> payload(100, 0x1);
    page.insert_record(payload.data(), static_cast<uint16_t>(payload.size()));

    size_t after = page.get_free_space();
    EXPECT_LT(after, before);
    EXPECT_EQ(before - after, payload.size() + sizeof(storage::Slot));
}

}  // namespace
