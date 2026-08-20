#ifndef DISTANCE_H
#define DISTANCE_H

#include <vector>
#include <cstddef>
#include <stdexcept>

namespace math {

// Standard Cosine Distance (Fallback / Non-AVX)
float cosine_distance_cpu(const float* a, const float* b, size_t dim);

// AVX2-Accelerated Cosine Distance
float cosine_distance_avx2(const float* a, const float* b, size_t dim);

// Euclidean (L2) Distance
float l2_distance_cpu(const float* a, const float* b, size_t dim);

// Unified wrapper function that selects the fastest supported instruction set
float calculate_cosine_distance(const float* a, const float* b, size_t dim);

} 

#endif // DISTANCE_H