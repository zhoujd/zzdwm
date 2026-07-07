FROM ubuntu:24.04

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies, Valgrind, and debug symbols
RUN apt-get update && apt-get install -y \
    build-essential \
    valgrind \
    libc6-dbg \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
