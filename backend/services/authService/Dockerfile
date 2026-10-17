# ============================================
# STAGE 1: Builder
# ============================================
ARG BUILD_TYPE=Release

FROM ghcr.io/userver-framework/ubuntu-22.04-userver:v2.15 AS builder
ARG BUILD_TYPE
ENV BUILD_TYPE=${BUILD_TYPE}

WORKDIR /app

COPY CMakeLists.txt .
COPY cmake ./cmake
COPY src ./src
COPY configs ./configs

#RUN find /usr -name "userver" -type d 2>/dev/null | head -5

RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DUSERVER_FEATURE_POSTGRESQL=ON \
    -G Ninja || true

RUN if [ "${BUILD_TYPE}" = "Debug" ]; then \
        cmake -B build \
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
            -DUSERVER_FEATURE_POSTGRESQL=ON \
            -DCMAKE_CXX_FLAGS="-g -O0 -fdebug-prefix-map=/app=/app" \
            -G Ninja; \
    else \
        cmake -B build \
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
            -DUSERVER_FEATURE_POSTGRESQL=ON \
            -G Ninja; \
    fi && \
    cmake --build build --parallel $(nproc) && \
    cmake --install build --prefix /install

RUN ldd /install/bin/db_test \
    | grep -oP '(?<=> )/[^ ]+' \
    | xargs -I{} cp --parents {} /install || true

# ============================================
# STAGE 2: Runtime
# ============================================
FROM ubuntu:22.04

ARG BUILD_TYPE=Release
ENV BUILD_TYPE=${BUILD_TYPE}

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libpq5 curl \
    && rm -rf /var/lib/apt/lists/*

RUN if [ "$BUILD_TYPE" = "Debug" ]; then \
        apt-get update && apt-get install -y gdbserver; \
    fi

RUN mkdir -p /app/config
WORKDIR /app

COPY --from=builder /install/bin/db_test /app/db_test
COPY --from=builder /install/etc/db_test /app/config
COPY --from=builder /install/lib/* /usr/local/lib/
RUN ldconfig

# ВАЖНО: Копируем исходники сервиса в контейнер (как во втором сервисе!)
COPY --from=builder /app/src /app/src

RUN chmod +x /app/db_test

EXPOSE 8080

ENV DB_CONNECTION=""

CMD if [ "$BUILD_TYPE" = "Debug" ]; then \
        echo "Starting gdbserver on port 12345" && \
        gdbserver :12345 /app/db_test -c /app/config/static_config.yaml --config_vars /app/config/config_vars.yaml; \
    else \
        /app/db_test -c /app/config/static_config.yaml --config_vars /app/config/config_vars.yaml; \
    fi