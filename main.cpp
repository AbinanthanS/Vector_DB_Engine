#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "storage/buffer_pool.h"
#include "query/engine.h"
#include "math/distance.h"

namespace {

using Clock = std::chrono::steady_clock;

struct Stats {
    double avg_ms{};
    double p50_ms{};
    double p95_ms{};
    double p99_ms{};
    double min_ms{};
    double max_ms{};
};

Stats summarize(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    const auto pct = [&](double p) {
        const size_t idx = std::min(
            samples.size() - 1,
            static_cast<size_t>(std::ceil(p * samples.size())) - 1);
        return samples[idx];
    };

    Stats s;
    s.avg_ms = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
    s.p50_ms = pct(0.50);
    s.p95_ms = pct(0.95);
    s.p99_ms = pct(0.99);
    s.min_ms = samples.front();
    s.max_ms = samples.back();
    return s;
}

template <typename Fn>
double time_ms(Fn&& fn) {
    const auto start = Clock::now();
    fn();
    const auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

int main(int argc, char** argv) {
    constexpr size_t DIM = 128;
    const size_t DATASET_SIZE = (argc > 1 && std::string(argv[1]) == "--large") ? 500'000 : 100'000;
    constexpr size_t NUM_QUERIES = 100;
    constexpr size_t K = 10;
    constexpr uint32_t NUM_CATEGORIES = 5;

    const std::string db_file = "benchmark_engine.db";
    std::error_code ec;
    std::filesystem::remove(db_file, ec);

    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> value_dist(0.0f, 1.0f);
    std::uniform_int_distribution<uint32_t> category_dist(0, NUM_CATEGORIES - 1);

    std::cout << "============================================================\n";
    std::cout << "             VECTOR DB ENGINE BENCHMARK SUITE\n";
    std::cout << "============================================================\n\n";
    std::cout << "Dataset: " << DATASET_SIZE << " vectors x " << DIM
              << "D FP32 | K=" << K << " | Queries=" << NUM_QUERIES << "\n\n";

    // ------------------------------------------------------------
    // 1. Math benchmark: scalar vs AVX2/FMA
    // ------------------------------------------------------------
    std::cout << "[1/4] Distance Layer Benchmark\n";

    std::vector<float> a(DIM), b(DIM);
    for (size_t i = 0; i < DIM; ++i) {
        a[i] = value_dist(rng);
        b[i] = value_dist(rng);
    }

    constexpr size_t MATH_ITERATIONS = 500'000;
    volatile float sink = 0.0f;

    const double scalar_ms = time_ms([&] {
        for (size_t i = 0; i < MATH_ITERATIONS; ++i) {
            sink += math::cosine_distance_cpu(a.data(), b.data(), DIM);
        }
    });

    const double simd_ms = time_ms([&] {
        for (size_t i = 0; i < MATH_ITERATIONS; ++i) {
            sink += math::cosine_distance_avx2(a.data(), b.data(), DIM);
        }
    });

    std::cout << "  Scalar cosine : " << scalar_ms << " ms\n";
    std::cout << "  AVX2/FMA      : " << simd_ms << " ms\n";
    std::cout << "  SIMD speedup  : " << scalar_ms / simd_ms << "x\n";
    std::cout << "  Throughput     : "
              << (MATH_ITERATIONS / (simd_ms / 1000.0)) / 1e6
              << " M distance ops/sec\n\n";

    // ------------------------------------------------------------
    // 2. Build dataset + storage/index ingestion
    // ------------------------------------------------------------
    std::cout << "[2/4] Ingestion Benchmark\n";
    storage::BufferPoolManager bpm(db_file, 1000);
    query::ExecutionEngine engine(bpm, DIM);

    // Deterministic, evenly distributed categories. This is important:
    // every category has real candidates, so filtered benchmarks don't
    // accidentally measure the zero-candidate fast path.
    std::vector<std::vector<float>> queries(NUM_QUERIES, std::vector<float>(DIM));
    std::vector<uint32_t> query_categories(NUM_QUERIES);

    for (auto& q : queries) {
        for (float& x : q) x = value_dist(rng);
    }
    for (auto& c : query_categories) c = category_dist(rng);

    const double ingest_ms = time_ms([&] {
        std::vector<float> vec(DIM);
        for (size_t i = 0; i < DATASET_SIZE; ++i) {
            for (float& x : vec) x = value_dist(rng);
            const uint32_t category = static_cast<uint32_t>(i % NUM_CATEGORIES);
            engine.insert_record(static_cast<uint32_t>(i), category, vec);
        }
    });

    std::cout << "  Records       : " << DATASET_SIZE << "\n";
    std::cout << "  Time           : " << ingest_ms << " ms\n";
    std::cout << "  Throughput     : "
              << DATASET_SIZE / (ingest_ms / 1000.0)
              << " records/sec\n\n";

    // ------------------------------------------------------------
    // 3. Warm-up + hybrid filtered retrieval
    // ------------------------------------------------------------
    std::cout << "[3/4] Hybrid Exact Filtered Top-K Benchmark\n";
    std::cout << "  Category distribution: 5 categories, ~"
              << DATASET_SIZE / NUM_CATEGORIES << " candidates/category\n";

    // Warm up code paths and caches. Warm-up samples are not timed.
    for (size_t i = 0; i < 10; ++i) {
        auto warm = engine.hybrid_query(query_categories[i], queries[i], K);
        if (warm.size() != K) {
            std::cerr << "ERROR: warm-up returned " << warm.size()
                      << " results instead of " << K << "\n";
            return 1;
        }
    }

    std::vector<double> latencies;
    latencies.reserve(NUM_QUERIES);
    size_t total_results = 0;

    for (size_t i = 0; i < NUM_QUERIES; ++i) {
        const double ms = time_ms([&] {
            auto results = engine.hybrid_query(
                query_categories[i], queries[i], K);
            total_results += results.size();

            if (results.size() != K) {
                std::cerr << "ERROR: query returned " << results.size()
                          << " results instead of " << K << "\n";
            }
        });
        latencies.push_back(ms);
    }

    const Stats filtered = summarize(latencies);
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  Avg latency   : " << filtered.avg_ms << " ms\n";
    std::cout << "  p50 latency   : " << filtered.p50_ms << " ms\n";
    std::cout << "  p95 latency   : " << filtered.p95_ms << " ms\n";
    std::cout << "  p99 latency   : " << filtered.p99_ms << " ms\n";
    std::cout << "  Min / Max     : " << filtered.min_ms << " / "
              << filtered.max_ms << " ms\n";
    std::cout << "  Results/query : "
              << static_cast<double>(total_results) / NUM_QUERIES << "\n\n";

    // ------------------------------------------------------------
    // 4. Empty-filter correctness/fast-path benchmark
    // ------------------------------------------------------------
    std::cout << "[4/4] Empty Filter Fast-Path Benchmark\n";
    const uint32_t missing_category = NUM_CATEGORIES + 100;
    std::vector<double> empty_latencies;
    empty_latencies.reserve(NUM_QUERIES);

    for (size_t i = 0; i < NUM_QUERIES; ++i) {
        const double ms = time_ms([&] {
            auto results = engine.hybrid_query(
                missing_category, queries[i], K);
            if (!results.empty()) {
                std::cerr << "ERROR: missing category returned results\n";
            }
        });
        empty_latencies.push_back(ms);
    }

    const Stats empty = summarize(empty_latencies);
    std::cout << "  Avg latency   : " << empty.avg_ms << " ms\n";
    std::cout << "  p95 latency   : " << empty.p95_ms << " ms\n";
    std::cout << "  p99 latency   : " << empty.p99_ms << " ms\n";

    std::cout << "\n============================================================\n";
    std::cout << "Notes:\n";
    std::cout << "  * Filtered search is exact over the B+ tree candidate set.\n";
    std::cout << "  * Top-K uses a bounded heap, not a full sort.\n";
    std::cout << "  * Empty-filter latency is reported separately and is NOT\n";
    std::cout << "    presented as vector-retrieval latency.\n";
    std::cout << "============================================================\n";

    (void)sink;
    return 0;
}
