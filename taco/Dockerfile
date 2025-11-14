FROM ubuntu:22.04

# Believe the only dependency for the cli is cmake
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt
RUN git clone https://github.com/tensor-compiler/taco.git

WORKDIR /opt/taco/build
RUN cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .. && \
    make -j$(nproc)

ENTRYPOINT ["/opt/taco/build/bin/taco"]
CMD []
