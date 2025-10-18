FROM ubuntu:24.04 AS build

# Update system and install required packages
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    cmake make clang libasio-dev python3

WORKDIR /app

COPY . /app

# Build the app
RUN cmake -B build && \
    cmake --build build


# Enter run stage
FROM ubuntu:24.04 AS run

WORKDIR /app

# Copy binary from build stage
COPY --from=build /app/build/financial_radar .

# Open port to accept transactions on endpoint
EXPOSE 80

# Run app on container start
ENTRYPOINT ["./financial_radar"]
