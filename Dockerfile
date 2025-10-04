FROM ubuntu:22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install essential build tools and development libraries
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    gdb \
    make \
    cmake \
    valgrind \
    strace \
    ltrace \
    vim \
    nano \
    git \
    linux-tools-common \
    linux-tools-generic \
    htop \
    traceroute \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace


# Set default command
CMD ["/bin/bash"]
