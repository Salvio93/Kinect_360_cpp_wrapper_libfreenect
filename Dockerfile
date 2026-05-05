
FROM arm64v8/debian:bookworm-slim

# Install dependencies
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    build-essential \
    libusb-1.0-0-dev \
    pkg-config \
    python3 \
    python3-pip \
    freeglut3-dev \
    libxmu-dev \
    libxi-dev \
    && rm -rf /var/lib/apt/lists/*

# Clone and build libfreenect
WORKDIR /opt
RUN git clone https://github.com/OpenKinect/libfreenect.git
WORKDIR /opt/libfreenect
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make && \
    make install && \
    ldconfig

# Set library path
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Create working directory for our app
WORKDIR /app

# Keep container running
CMD ["/bin/bash"]
