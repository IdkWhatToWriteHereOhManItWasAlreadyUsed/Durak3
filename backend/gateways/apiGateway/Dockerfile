# Owline ============================================
# STAGE 1: Builder
# ============================================
ARG BUILD_TYPE=Release

FROM ubuntu:22.04 AS builder
ARG BUILD_TYPE
ENV BUILD_TYPE=${BUILD_TYPE}

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    g++ \
    libssl-dev \
    libcurl4-openssl-dev \
    libuuid1 \
    uuid-dev \
    zlib1g-dev \
    libjsoncpp-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/drogonframework/drogon.git && \
    cd drogon && \
    git submodule update --init && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release \
             -DBUILD_EXAMPLES=OFF \
             -DBUILD_CTL=OFF \
             -DBUILD_POSTGRESQL=OFF \
             -DBUILD_MYSQL=OFF \
             -DBUILD_SQLITE3=OFF \
             -DUSE_ORM=OFF \
             -DBUILD_REDIS=ON \
             -DUSE_BROTLI=OFF \
             && make -j$(nproc) && make install && ldconfig

COPY CMakeLists.txt /build/
COPY src /build/src
COPY config /build/config

WORKDIR /build
RUN mkdir -p /build/build && cd /build/build && \
    if [ "${BUILD_TYPE}" = "Debug" ]; then \
        cmake .. -DCMAKE_BUILD_TYPE=Debug -G Ninja \
                 -DCMAKE_CXX_FLAGS="-g -O0 -fdebug-prefix-map=/build=/app"; \
    else \
        cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja; \
    fi && \
    make -j$(nproc)

# ============================================
# STAGE 2: Runtime
# ============================================
FROM ubuntu:22.04

ARG BUILD_TYPE=Release
ENV BUILD_TYPE=${BUILD_TYPE}

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    libssl3 \
    libcurl4 \
    zlib1g \
    libjsoncpp25 \
    libhiredis0.14 \
    && rm -rf /var/lib/apt/lists/*

RUN if [ "$BUILD_TYPE" = "Debug" ]; then \
        apt-get update && apt-get install -y --no-install-recommends gdbserver; \
    fi

COPY --from=builder /usr/local/lib/libdrogon* /usr/local/lib/
COPY --from=builder /usr/local/lib/libtrantor* /usr/local/lib/
RUN ldconfig

RUN mkdir -p /app/config

WORKDIR /app

COPY --from=builder /build/build/apiGateway /app/apiGateway
RUN chmod +x /app/apiGateway

COPY config /app/config

EXPOSE 8080

CMD if [ "$BUILD_TYPE" = "Debug" ]; then \
        gdbserver :12345 /app/apiGateway; \
    else \
        /app/apiGateway; \
    fi
