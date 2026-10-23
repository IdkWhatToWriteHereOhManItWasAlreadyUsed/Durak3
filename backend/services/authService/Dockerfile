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

RUN if [ "${BUILD_TYPE}" = "Debug" ]; then \
        cmake -B build \
            -DCMAKE_BUILD_TYPE=Debug \
            -DUSERVER_FEATURE_POSTGRESQL=ON \
            -DCMAKE_CXX_FLAGS="-g -O0 -fdebug-prefix-map=/app=/app"; \
    else \
        cmake -B build \
            -DCMAKE_BUILD_TYPE=Release \
            -DUSERVER_FEATURE_POSTGRESQL=ON; \
    fi && \
    cmake --build build --parallel 4 && \
    cmake --install build --prefix /install && \
    find /install -type f | sort

# Подтянуть разделяемые библиотеки, которые не попали в /install
RUN ldd /install/bin/authService \
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
        apt-get update && apt-get install -y --no-install-recommends gdbserver \
        && rm -rf /var/lib/apt/lists/*; \
    fi

RUN mkdir -p /app/config
WORKDIR /app

COPY --from=builder /install/bin/authService /app/authService
COPY --from=builder /install/etc/authService /app/config
COPY --from=builder /install/lib/* /usr/local/lib/
RUN ldconfig

# Если реально нужны исходники в runtime (обычно нет — можно удалить)
COPY --from=builder /app/src /app/src

RUN chmod +x /app/authService

EXPOSE 8080

ENV DB_CONNECTION=""

CMD if [ "$BUILD_TYPE" = "Debug" ]; then \
        echo "Starting gdbserver on port 12345" && \
        gdbserver :12345 /app/authService -c /app/config/static_config.yaml --config_vars /app/config/config_vars.yaml; \
    else \
        /app/authService -c /app/config/static_config.yaml --config_vars /app/config/config_vars.yaml; \
    fi