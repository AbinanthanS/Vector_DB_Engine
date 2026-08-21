#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <numeric>
#include <algorithm>
#include "storage/buffer_pool.h"
#include "query/engine.h"
#include "math/distance.h"

int main() {
    std::cout << "=========================================================" << std::endl;
    std::cout << "       VECTOR DB ENGINE BENCHMARK SUITE (C++20)          " << std::endl;
    std::cout << "=========================================================\n" << std::endl;

    constexpr size_t DIM = 128;
    constexpr size_t NUM_RECORDS = 5000;
    const std::string db_file = "benchmark_engine.db";

    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::uniform_int_distribution<uint32_t> category_dist(1, 5);

    // 1. Math Layer SIMD Benchmark
    std::cout << "[1/3] Benchmarking Math Layer (AVX2 SIMD vs Scalar)..." << std::endl;
    std::vector<float> vec_a(DIM), vec_b(DIM);
    for (size_t i = 0; i < DIM; ++i) {
        vec_a[i] = dist(rng);
        vec_b[i] = dist(rng);
    }

    constexpr size_t MATH_ITERATIONS = 500000;
    auto start_math = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < MATH_ITERATIONS; ++i) {
        volatile float d = math::calculate_cosine_distance(vec_a.data(), vec_b.data(), DIM);
    }
    auto end_math = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> math_duration = end_math - start_math;
    std::cout << " -> Computed " << MATH_ITERATIONS << " Cosine Distances in: " 
              << math_duration.count() << " ms (" 
              << (MATH_ITERATIONS / (math_duration.count() / 1000.0)) / 1e6 << " M ops/sec)\n" << std::endl;

    // 2. Storage & Index Insertion Benchmark
    std::cout << "[2/3] Benchmarking Storage & Dual-Index Ingestion (" << NUM_RECORDS << " records)..." << std::endl;
    storage::BufferPoolManager bpm(db_file, 100);
    query::ExecutionEngine engine(bpm, DIM);

    auto start_ingest = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < NUM_RECORDS; ++i) {
        std::vector<float> vec(DIM);
        for (size_t d_idx = 0; d_idx < DIM; ++d_idx) vec[d_idx] = dist(rng);
        uint32_t category = category_dist(rng);
        engine.insert_record(static_cast<uint32_t>(i), category, vec);
    }
    auto end_ingest = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ingest_duration = end_ingest - start_ingest;
    std::cout << " -> Ingested " << NUM_RECORDS << " vectors in: " 
              << ingest_duration.count() << " ms ("
              << (NUM_RECORDS / (ingest_duration.count() / 1000.0)) << " Records/sec)\n" << std::endl;

    // 3. Hybrid Query Latency Benchmark (p95 / p99)
    std::cout << "[3/3] Benchmarking Hybrid Query Latency (Top-10 retrieval with Category Filter)..." << std::endl;
    constexpr size_t NUM_QUERIES = 200;
    std::vector<double> latencies_ms;

    for (size_t q = 0; q < NUM_QUERIES; ++q) {
        std::vector<float> query_vec(DIM);
        for (size_t d_idx = 0; d_idx < DIM; ++d_idx) query_vec[d_idx] = dist(rng);
        uint32_t target_category = category_dist(rng);

        auto start_q = std::chrono::high_resolution_clock::now();
        auto results = engine.hybrid_query(target_category, query_vec, 10);
        auto end_q = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> q_dur = end_q - start_q;
        latencies_ms.push_back(q_dur.count());
    }

    std::sort(latencies_ms.begin(), latencies_ms.end());
    double avg_lat = std::accumulate(latencies_ms.begin(), latencies_ms.end(), 0.0) / NUM_QUERIES;
    double p95_lat = latencies_ms[static_cast<size_t>(NUM_QUERIES * 0.95)];
    double p99_lat = latencies_ms[static_cast<size_t>(NUM_QUERIES * 0.99)];

    std::cout << " -> Avg Query Latency: " << avg_lat << " ms" << std::endl;
    std::cout << " -> p95 Query Latency: " << p95_lat << " ms" << std::endl;
    std::cout << " -> p99 Query Latency: " << p99_lat << " ms" << std::endl;
    std::cout << "\n=========================================================" << std::endl;

    return 0;
}