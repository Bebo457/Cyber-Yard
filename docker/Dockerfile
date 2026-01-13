# =============================================================================
# Cyber-Yard / Scotland Yard++ Docker Container
# =============================================================================
# Single container for running the game graphically with full Python support.
# Supports multiplayer over LAN/public networks (ENet UDP ports).
# =============================================================================

ARG UBUNTU_VERSION=24.04
FROM ubuntu:${UBUNTU_VERSION} AS builder

# Disable interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    git \
    libsdl2-dev \
    libglew-dev \
    libgl1-mesa-dev \
    libglm-dev \
    libfreetype6-dev \
    libenet-dev \
    libzmq3-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source code
WORKDIR /app
COPY program/ ./program/

# Build the game
WORKDIR /app/program/build
RUN cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# =============================================================================
# Runtime Stage
# =============================================================================
FROM ubuntu:${UBUNTU_VERSION}

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Install runtime dependencies and Python
RUN apt-get update && apt-get install -y \
    # OpenGL/Graphics runtime
    libsdl2-2.0-0 \
    libglew2.2 \
    libgl1 \
    libglx-mesa0 \
    libgl1-mesa-dri \
    libopengl0 \
    libfreetype6 \
    # Networking
    libenet7 \
    libzmq5 \
    # X11 support
    x11-apps \
    mesa-utils \
    # Python
    python3 \
    python3-pip \
    python3-venv \
    && rm -rf /var/lib/apt/lists/*

# Install Python packages for AI scripts
RUN pip3 install --break-system-packages \
    torch \
    numpy \
    pyzmq \
    pandas \
    matplotlib \
    seaborn

# Environment variables for GPU acceleration
ENV NVIDIA_VISIBLE_DEVICES=all
ENV NVIDIA_DRIVER_CAPABILITIES=graphics,compute,utility

# Create directory structure matching what the game expects
WORKDIR /app/program

# Copy built binary from builder stage
COPY --from=builder /app/program/build/bin/ /app/program/build/bin/

# Copy assets and scripts with correct paths
COPY program/assets/ /app/program/assets/
COPY program/scripts/ /app/program/scripts/

# Copy map data
COPY map/ /app/map/

# Expose ports for multiplayer
# UDP 12345-12346: ENet multiplayer
# TCP 5555-5556: ZeroMQ Python bridge
EXPOSE 12345/udp 12346/udp 5555 5556

# Set working directory
WORKDIR /app/program/build/bin

# Run the game directly (Python scripts are now launched by the game itself)
CMD ["./ScotlandYardPlusPlus"]
