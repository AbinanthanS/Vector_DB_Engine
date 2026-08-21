#include <gtest/gtest.h>
#include <cmath>
#include <random>
#include <vector>

#include "math/distance.h"

namespace {

constexpr float kEps = 1e-4f;

TEST(Distance, IdenticalVectorsHaveZeroCosineDistance) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_NEAR(math::cosine_distance_cpu(a.data(), a.data(), a.size()), 0.0f, kEps);
    EXPECT_NEAR(math::cosine_distance_avx2(a.data(), a.data(), a.size()), 0.0f, kEps);
}

TEST(Distance, OrthogonalVectorsHaveUnitCosineDistance) {
    std::vector<float> a = {1.0f, 0.0f};
    std::vector<float> b = {0.0f, 1.0f};
    EXPECT_NEAR(math::cosine_distance_cpu(a.data(), b.data(), 2), 1.0f, kEps);
}

TEST(Distance, OppositeVectorsHaveDistanceTwo) {
    std::vector<float> a = {1.0f, 0.0f};
    std::vector<float> b = {-1.0f, 0.0f};
    EXPECT_NEAR(math::cosine_distance_cpu(a.data(), b.data(), 2), 2.0f, kEps);
}

TEST(Distance, ZeroVectorReturnsSentinelDistance) {
    std::vector<float> zero = {0.0f, 0.0f, 0.0f};
    std::vector<float> other = {1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(math::cosine_distance_cpu(zero.data(), other.data(), 3), 1.0f);
    EXPECT_FLOAT_EQ(math::cosine_distance_avx2(zero.data(), other.data(), 3), 1.0f);
}

TEST(Distance, ScalarAndAvx2AgreeOnRandomVectorsWholeBlocks) {
    // dim is a multiple of 8: exercises only the main AVX2 loop.
    constexpr size_t dim = 128;
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> dist(-5.0f, 5.0f);
    std::vector<float> a(dim), b(dim);
    for (size_t i = 0; i < dim; ++i) { a[i] = dist(rng); b[i] = dist(rng); }

    float scalar = math::cosine_distance_cpu(a.data(), b.data(), dim);
    float simd = math::cosine_distance_avx2(a.data(), b.data(), dim);
    EXPECT_NEAR(scalar, simd, kEps);
}

TEST(Distance, ScalarAndAvx2AgreeOnRandomVectorsWithTail) {
    // dim is NOT a multiple of 8: exercises the AVX2 tail loop.
    constexpr size_t dim = 131;
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> dist(-5.0f, 5.0f);
    std::vector<float> a(dim), b(dim);
    for (size_t i = 0; i < dim; ++i) { a[i] = dist(rng); b[i] = dist(rng); }

    float scalar = math::cosine_distance_cpu(a.data(), b.data(), dim);
    float simd = math::cosine_distance_avx2(a.data(), b.data(), dim);
    EXPECT_NEAR(scalar, simd, kEps);
}

TEST(Distance, CalculateCosineDistanceMatchesDirectImplementation) {
    constexpr size_t dim = 64;
    std::mt19937 rng(3);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> a(dim), b(dim);
    for (size_t i = 0; i < dim; ++i) { a[i] = dist(rng); b[i] = dist(rng); }

    float wrapper = math::calculate_cosine_distance(a.data(), b.data(), dim);
    float scalar = math::cosine_distance_cpu(a.data(), b.data(), dim);
    EXPECT_NEAR(wrapper, scalar, kEps);
}

TEST(Distance, L2DistanceKnownValue) {
    std::vector<float> a = {0.0f, 0.0f};
    std::vector<float> b = {3.0f, 4.0f};
    EXPECT_NEAR(math::l2_distance_cpu(a.data(), b.data(), 2), 5.0f, kEps);
}

TEST(Distance, L2DistanceZeroForIdenticalVectors) {
    std::vector<float> a = {1.5f, -2.25f, 3.75f};
    EXPECT_NEAR(math::l2_distance_cpu(a.data(), a.data(), 3), 0.0f, kEps);
}

}  // namespace
