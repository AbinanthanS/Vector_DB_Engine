# Vector DB Engine

A compact C++20 vector database engine built from scratch for storage, indexing,
and approximate nearest-neighbor search experiments. The current implementation
includes:

- slotted pages backed by a buffer pool
- a B+ tree for category posting lists
- an HNSW graph for approximate nearest-neighbor search
- scalar and AVX2/FMA cosine-distance implementations
- an exact filtered top-k query path over B+ tree candidates

## Requirements

- CMake 3.20 or newer
- a C++20 compiler
- AVX2/FMA support at runtime for the SIMD distance benchmark
- Internet access on the first configure when tests need to fetch GoogleTest

The CMake configuration enables `/arch:AVX2` on MSVC and `-mavx2 -mfma -O3`
on other compilers. Run the SIMD benchmark only on a machine that supports
those instructions.

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

To build without the GoogleTest suite:

```bash
cmake -S . -B build -DBUILD_TESTS=OFF
cmake --build build --config Release
```

## Run

The executable runs the default 100,000-vector benchmark. The `--large` option
uses 500,000 vectors.

```bash
./build/vectordb
./build/vectordb --large
```

On Windows with a multi-config generator, use the Release output directory:

```powershell
.\build\Release\vectordb.exe
.\build\Release\vectordb.exe --large
```

Each run creates `benchmark_engine.db` in the working directory and removes an
existing file with that name before loading the synthetic dataset.

## Docker

Docker is optional. Use it when you want to run the benchmark in the pinned
Ubuntu 24.04 environment defined by the `Dockerfile`.

Build the image from the project root:

```bash
docker build -t vectordb .
```

Run the default benchmark:

```bash
docker run --rm vectordb
```

Run the 500,000-vector workload:

```bash
docker run --rm vectordb --large
```

The container writes `benchmark_engine.db` under `/app/data`. Mount a local
directory when you want to keep that file after the container exits:

```bash
docker run --rm -v "${PWD}/data:/app/data" vectordb
```

The image builds the benchmark without tests by default. To include the test
binary, build with `BUILD_TESTS=ON`; GoogleTest is fetched during this build:

```bash
docker build --build-arg BUILD_TESTS=ON -t vectordb-tests .
```

The image enables AVX2/FMA during compilation and therefore requires a
compatible CPU at runtime.

## Benchmark

The benchmark uses deterministic random data (`mt19937`, seed `1337`) with
128-dimensional FP32 vectors, `K=10`, 100 timed queries, and five evenly
distributed categories. It reports:

1. scalar versus AVX2/FMA cosine distance over 500,000 iterations
2. storage and index ingestion throughput
3. exact filtered top-k latency, including average, p50, p95, and p99
4. the missing-category fast path, reported separately from retrieval latency

### Baseline Result

Measured on the current workspace build using the default workload. Results are
machine-dependent and should be treated as a reference, not a performance
guarantee.

| Metric | Result |
| --- | ---: |
| Dataset | 100,000 vectors x 128D FP32 |
| Scalar cosine | 74.9413 ms / 500,000 ops |
| AVX2/FMA cosine | 16.2498 ms / 500,000 ops |
| SIMD speedup | 4.61x |
| SIMD throughput | 30.7696 M distance ops/sec |
| Ingestion throughput | 118,795 records/sec |
| Filtered top-k average | 8.7067 ms |
| Filtered top-k p50 | 8.0771 ms |
| Filtered top-k p95 | 14.6883 ms |
| Filtered top-k p99 | 15.3577 ms |
| Filtered results/query | 10 |
| Empty-category average | 0.0014 ms |

The filtered search is exact over the B+ tree candidate set. Top-k selection
uses a bounded heap rather than sorting every candidate. The benchmark does
not measure concurrent clients, durability under failure, recall against an
exact unfiltered ground truth, or a production-scale workload.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

The test suite covers slotted-page operations, buffer-pool allocation and
flushing, B+ tree lookups and splits, distance calculations, HNSW search, and
query-engine filtering.

## Project Layout

```text
include/       Public headers grouped by subsystem
src/           Storage, index, distance, and query implementations
tests/         GoogleTest unit tests
main.cpp       Synthetic benchmark executable
index/         Index-related headers retained for the project layout
math/          Distance-related headers retained for the project layout
query/         Query-engine headers retained for the project layout
storage/       Storage headers retained for the project layout
```

This project is intended for experimentation and performance profiling rather
than production deployment.