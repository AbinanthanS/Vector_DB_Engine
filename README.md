# Vector DB Engine

A compact C++ vector database engine built from scratch with:

- a custom slotted-page storage layer
- a B+ tree for category-based filtering
- an HNSW graph for ANN/vector search
- AVX2/FMA cosine-distance acceleration
- an exact hybrid query path using the B+ tree posting list

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run benchmark

```bash
./build/vectordb
./build/vectordb --large
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Notes

The benchmark binary performs a realistic end-to-end workload:

- building a synthetic dataset
- ingesting vectors into the storage engine
- exact filtered top-k lookup using the B+ tree candidate set
- empty-filter fast-path benchmarking

This project is designed for experimentation and performance profiling rather than production deployment.