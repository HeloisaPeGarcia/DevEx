# Build stage
FROM alpine:3.19 AS builder

RUN apk add --no-cache g++ make cmake

WORKDIR /build
COPY DevEx/ /build/

RUN g++ -std=c++20 -O3 -pthread \
    DevEx.cpp \
    TextUtil.cpp \
    Terminal.cpp \
    CredentialVault.cpp \
    GitProvider.cpp \
    EnvironmentStore.cpp \
    Orchestrator.cpp \
    OrbitDesktopApp.cpp \
    -o orbit-agent

# Run stage
FROM alpine:3.19

RUN apk add --no-cache libstdc++ libgcc

WORKDIR /app
COPY --from=builder /build/orbit-agent /app/orbit-agent

# Set environment variables defaults
ENV ORBIT_STORE_PATH="/app/data/environments.tsv"
ENV ORBIT_TOKEN=""

# Expose data directory for persistent mounting
VOLUME /app/data

ENTRYPOINT ["/app/orbit-agent"]
CMD ["--help"]
