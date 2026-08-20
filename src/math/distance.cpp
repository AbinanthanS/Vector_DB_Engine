#include "math/distance.h"
#include <cmath>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

namespace math {

float cosine_distance_cpu(const float* a, const float* b, size_t dim) {
    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (size_t i = 0; i < dim; ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    if (norm_a == 0.0f || norm_b == 0.0f) return 1.0f; // Prevent div by zero
    float similarity = dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b));
    return 1.0f - similarity; // Convert similarity to distance metric
}

float cosine_distance_avx2(const float* a, const float* b, size_t dim) {
#if defined(__AVX2__)
    __m256 vdot = _mm256_setzero_ps();
    __m256 vnorm_a = _mm256_setzero_ps();
    __m256 vnorm_b = _mm256_setzero_ps();

    size_t i = 0;
    // Process 8 floats per SIMD instruction
    for (; i + 7 < dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);

        vdot = _mm256_fmadd_ps(va, vb, vdot);
        vnorm_a = _mm256_fmadd_ps(va, va, vnorm_a);
        vnorm_b = _mm256_fmadd_ps(vb, vb, vnorm_b);
    }

    // Accumulate horizontal vector elements
    alignas(32) float buffer_dot[8], buffer_a[8], buffer_b[8];
    _mm256_storeu_ps(buffer_dot, vdot);
    _mm256_storeu_ps(buffer_a, vnorm_a);
    _mm256_storeu_ps(buffer_b, vnorm_b);

    float dot_product = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (int j = 0; j < 8; ++j) {
        dot_product += buffer_dot[j];
        norm_a += buffer_a[j];
        norm_b += buffer_b[j];
    }

    // Handle leftover elements tail loop
    for (; i < dim; ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    if (norm_a == 0.0f || norm_b == 0.0f) return 1.0f;
    return 1.0f - (dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b)));
#else
    return cosine_distance_cpu(a, b, dim);
#endif
}

float calculate_cosine_distance(const float* a, const float* b, size_t dim) {
#if defined(__AVX2__)
    return cosine_distance_avx2(a, b, dim);
#else
    return cosine_distance_cpu(a, b, dim);
#endif
}

}