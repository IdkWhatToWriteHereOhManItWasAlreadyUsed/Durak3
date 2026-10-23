# ============================================
# STAGE 1: Builder
# ============================================
ARG BUILD_TYPE=Debug

FROM ghcr.io/userver-framework/ubuntu-22.04-userver:latest AS builder
ARG BUILD_TYPE
ENV BUILD_TYPE=${BUILD_TYPE}

WORKDIR /app

# Копируем всё, что нужно CMake на этапе конфигурации
COPY CMakeLists.txt .
COPY cmake ./cmake
COPY openapi.yaml ./openapi.yaml
COPY configs ./configs
COPY src ./src

# Теперь конфигурируем
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DUSERVER_FEATURE_MONGODB=OFF \
    -DUSERVER_FEATURE_POSTGRESQL=OFF \
    -DUSERVER_FEATURE_GRPC=OFF \
    -DUSERVER_FEATURE_REDIS=ON \
    -DUSERVER_FEATURE_UTEST=ON

RUN cmake --build build --parallel 4 && \
    cmake --install build --prefix /install

RUN ldd /install/bin/gameService \
    | grep -oP '(?<=> )/[^ ]+' \
    | xargs -I{} cp --parents {} /install || true

# ============================================
# STAGE 2: Runtime
# ============================================
FROM ubuntu:22.04

ARG BUILD_TYPE=Debug
ENV BUILD_TYPE=${BUILD_TYPE}

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /install/bin/gameService /usr/local/bin/gameService
COPY --from=builder /install/etc/gameService /etc/gameService
COPY --from=builder /install/lib/* /usr/local/lib/
RUN ldconfig

EXPOSE 8080

HEALTHCHECK --interval=10s --timeout=3s --start-period=30s --retries=3 \
    CMD curl -f http://localhost:8080/ping || exit 1

CMD /usr/local/bin/gameService -c /etc/gameService/static_config.yaml --config_vars /etc/gameService/config_vars.docker.yaml