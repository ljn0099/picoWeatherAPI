# Stage 1: build
FROM docker.io/library/alpine:3.22 AS builder

RUN apk add --no-cache \
    build-base \
    cmake \
    pkgconf \
    libpq-dev \
    libmicrohttpd-dev \
    jansson-dev \
    libsodium-dev \
    flex

WORKDIR /api
COPY src ./src
COPY CMakeLists.txt .

RUN mkdir -p build && cd build && cmake .. && make -j$(nproc)

# Stage 2: runtime
FROM docker.io/library/alpine:3.22

RUN apk add --no-cache \
    libpq \
    libmicrohttpd \
    jansson \
    libsodium

RUN addgroup -S apigroup && adduser -S apiuser -G apigroup

WORKDIR /api

COPY --from=builder /api/build/picoWeatherAPI ./picoWeatherAPI

RUN chown -R apiuser:apigroup /api

ENV DB_HOST=db \
    DB_PORT=5432 \
    DB_NAME=weather \
    DB_USER=weatherAPI \
    DB_PASS=weatherAPI \
    API_PORT=8080

USER apiuser

CMD ["./picoWeatherAPI"]
