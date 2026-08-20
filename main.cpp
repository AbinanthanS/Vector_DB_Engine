#include <iostream>
#include <vector>
#include <chrono>
#include "math/distance.h"

int main() {
    constexpr size_t DIM = 128;
    constexpr size_t ITERATIONS = 100000;

    std::vector<float> vec_a(DIM, 0.5f);
    std::vector<float> vec_b(DIM, 0.3f);

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < ITERATIONS; ++i) {
        volatile float dist = math::calculate_cosine_distance(vec_a.data(), vec_b.data(), DIM);
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "Executed " << ITERATIONS << " 128-dim Cosine operations in: " 
              << duration.count() << " ms" << std::endl;

    return 0;
}