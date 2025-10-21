FROM ubuntu:24.04 AS build

# Update system and install required packages
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    cmake make clang python3 libasio-dev libspdlog-dev libssl-dev

WORKDIR /app

COPY . /app

# Build the app
RUN cmake -B build && \
    cmake --build build

# Open port to accept transactions on endpoint
EXPOSE 80

ENTRYPOINT ["/app/build/financial_radar"]
