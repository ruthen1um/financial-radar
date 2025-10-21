# Libs need to be build before app
FROM ubuntu:24.04 AS build_deps

# Update system and install required system packages
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    ca-certificates \
    libasio-dev \
    cmake \
    clang \
    make \
    git

WORKDIR /tmp

RUN git clone https://github.com/CrowCpp/Crow.git && \
    cd Crow && \
    cmake -B build \
        -DCROW_ENABLE_COMPRESSION=FALSE \
        -DCROW_ENABLE_SSL=FALSE \
        -DCROW_BUILD_EXAMPLES=FALSE \
        -DCROW_BUILD_TESTS=FALSE && \
    cmake --build build --target install && \
    cd /tmp && rm -rf Crow

RUN git clone https://github.com/gabime/spdlog.git && \
    cd spdlog && \
    cmake -B build && \
    cmake --build build --target install && \
    cd /tmp && rm -rf spdlog

RUN git clone https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git && \
    cd AMQP-CPP && \
    cmake -B build && \
    cmake --build build --target install && \
    cd /tmp && rm -rf AMQP-CPP


# Build the app itself
FROM ubuntu:24.04 AS build_app

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    libasio-dev \
    cmake \
    clang \
    make

WORKDIR /app

COPY --from=build_deps /usr/local/cmake /usr/local/cmake
COPY --from=build_deps /usr/local/lib /usr/local/lib
COPY --from=build_deps /usr/local/include /usr/local/include

COPY CMakeLists.txt /app/CMakeLists.txt
COPY src /app/src

RUN cmake -B build
RUN cmake --build build

# Run the app
FROM ubuntu:24.04 AS runtime

WORKDIR /app

COPY --from=build_app /app/build/financial_radar /app/financial_radar

EXPOSE 80

ENTRYPOINT ["/app/financial_radar"]
