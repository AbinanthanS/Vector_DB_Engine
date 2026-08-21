# syntax=docker/dockerfile:1

# ------------------------------------------------------------
# Build stage
# ------------------------------------------------------------
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake \
        g++ \
        ninja-build \
        ca-certificates \
        git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# BUILD_TESTS is off for the image build by default to keep the image build
# from depending on network access to fetch GoogleTest; enable it with
# `docker build --build-arg BUILD_TESTS=ON .` if you want the test binary
# built inside the image too.
ARG BUILD_TESTS=OFF
RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=${BUILD_TESTS} \
    && cmake --build build -j"$(nproc)"

# ------------------------------------------------------------
# Runtime stage
# ------------------------------------------------------------
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --shell /bin/bash vectordb \
    && mkdir -p /app/data \
    && chown -R vectordb:vectordb /app

COPY --from=builder /src/build/vectordb /app/vectordb

USER vectordb
WORKDIR /app/data

# The benchmark binary writes its DB file to the current working directory.
VOLUME ["/app/data"]

ENTRYPOINT ["/app/vectordb"]
