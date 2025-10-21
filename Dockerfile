FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /app

# Install build deps (removed libsimpleamqpclient-dev)
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      cmake make clang python3 pkg-config build-essential git ca-certificates \
      libasio-dev libspdlog-dev libssl-dev \
      librabbitmq-dev \
      libboost-chrono-dev libboost-system-dev libboost-date-time-dev libboost-filesystem-dev \
    && rm -rf /var/lib/apt/lists/*


# Copy repo into image
COPY . /app

# Create build dir and configure & build
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel $(nproc)

EXPOSE 80

ENTRYPOINT ["/app/build/financial_radar"]
