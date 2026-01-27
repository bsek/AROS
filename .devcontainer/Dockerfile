FROM mcr.microsoft.com/devcontainers/base:ubuntu-22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive
ENV PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

# Install AROS build dependencies
RUN apt-get update && apt-get install -y \
    # Build essentials
    build-essential \
    gcc \
    g++ \
    # Build tools
    make \
    cmake \
    automake \
    autoconf \
    bison \
    flex \
    gawk \
    gperf \
    nasm \
    ccache \
    # Graphics and media libraries
    libpng-dev \
    zlib1g-dev \
    libxcursor-dev \
    libgl1-mesa-dev \
    libasound2-dev \
    # Image tools
    netpbm \
    # ISO/disk tools
    genisoimage \
    mtools \
    xorriso \
    # Archive tools
    jlha-utils \
    liblzma-dev \
    # Python
    python3 \
    python3-mako \
    python3-distutils-extra \
    # Perl
    libswitch-perl \
    # Utilities
    wget \
    curl \
    git \
    coreutils \
    ca-certificates \
    gnupg \
    && rm -rf /var/lib/apt/lists/*

# Install Node.js 20 LTS from NodeSource
RUN mkdir -p /etc/apt/keyrings \
    && curl -fsSL https://deb.nodesource.com/gpgkey/nodesource-repo.gpg.key | gpg --dearmor -o /etc/apt/keyrings/nodesource.gpg \
    && echo "deb [signed-by=/etc/apt/keyrings/nodesource.gpg] https://deb.nodesource.com/node_20.x nodistro main" | tee /etc/apt/sources.list.d/nodesource.list \
    && apt-get update \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

# Create build directory
RUN mkdir -p /home/vscode/build && chown vscode:vscode /home/vscode/build

# Set working directory
WORKDIR /workspaces/AROS
