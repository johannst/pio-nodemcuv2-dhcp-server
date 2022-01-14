// Copyright (c) 2022 Johannes Stoelp

#include <types.h>
#include <utils.h>

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <vector>

using ID = std::array<u8, 6>;
namespace fs = std::filesystem;

std::vector<ID> read_blob() {
    if (!fs::exists("blob")) {
        std::system("dd if=/dev/urandom of=blob count=16 bs=1M");
    }

    std::vector<ID> ids;
    ids.reserve(fs::file_size("blob") / sizeof(ID));

    auto ifs = std::ifstream("blob");
    assert(ifs.is_open());
    ID id;
    while (ifs.read((char*)id.data(), sizeof(id))) {
        ids.push_back(id);
    }

    return ids;
}

TEST(hash, uniform_distribuation) {
    constexpr usize BUCKETS = 64;
    constexpr float BUCKET_SIZE = static_cast<float>(100) / BUCKETS;  // Bucket size in percent.
    constexpr float BUCKET_ERR = BUCKET_SIZE * 0.05 /* 5% */;         // Allowed distribution error.

    const auto ids = read_blob();

    usize cnt[BUCKETS] = {0};
    for (const auto& id : ids) {
        u32 h = hash(id.data(), id.size());
        cnt[h % BUCKETS] += 1;
    }

    for (usize b = 0; b < BUCKETS; ++b) {
        const float dist = static_cast<float>(cnt[b]) / ids.size() * 100;
        ASSERT_GT(dist, BUCKET_SIZE - BUCKET_ERR);
        ASSERT_LT(dist, BUCKET_SIZE + BUCKET_ERR);
        // printf("bucket %2ld: %5.2f (%ld)\n", b, dist, cnt[b]);
    }
}

TEST(hash, DISABLED_collisions) {
    const auto ids = read_blob();

    std::unordered_map<u32, usize> hits;
    for (const auto& id : ids) {
        u32 h = hash(id.data(), id.size());
        hits[h] = hits[h] + 1;
    }

    usize collisions = 0;
    for (const auto& hit : hits) {
        if (hit.second > 1) {
            ++collisions;
        }
    }

    printf("Hashed %ld values got %ld collisions\n", ids.size(), collisions);
}
