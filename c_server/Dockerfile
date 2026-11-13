# Stage 1: Build SHE (Secure HTTP Engine) in C11
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc make libc6-dev zlib1g-dev libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

RUN make clean && make && make test

# Stage 2: Minimal hardened runtime image
FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    zlib1g libssl3 ca-certificates curl \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --no-create-home she

WORKDIR /opt/she
COPY --from=builder /build/she /opt/she/she
COPY --from=builder /build/she.conf /opt/she/she.conf
COPY --from=builder /build/www /opt/she/www

RUN mkdir -p /opt/she/logs && chown -R she:she /opt/she

USER she
EXPOSE 8899

HEALTHCHECK --interval=10s --timeout=3s --start-period=5s --retries=3 \
  CMD curl -fsS http://127.0.0.1:8899/cloud-mesh/healthz || exit 1

ENTRYPOINT ["/opt/she/she"]
CMD ["-c", "/opt/she/she.conf", "-p", "8899", "-d", "/opt/she/www"]
