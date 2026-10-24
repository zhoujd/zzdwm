## README

### Custom Linux RootFS Builder with AI Agent Core

An automated workspace designed to build, configure, and package a highly
compact, security-oriented custom Linux root filesystem (rootfs.tar.gz)
featuring an embedded, single-binary AI Agent CLI/TUI interface. 

### Architecture Overview

This project uses an optimized **Infrastructure as Code (IaC)** pattern
to manage filesystems. Instead of tracking binary trees or system nodes
in Git, it relies on configuration blueprints and automated scripts to
assemble a clean workspace from scratch. 

                  ┌────────────────────────────────────────┐
                  │          git code repository           │
                  │ (configs, overlay components, scripts) │
                  └───────────────────┬────────────────────┘
                                      │
                                      ▼ [make]
                  ┌────────────────────────────────────────┐
                  │        automated assembly block        │
                  │   (pulls base alpine & applies layers) │
                  └───────────────────┬────────────────────┘
                                      │
                                      ▼
                  ┌────────────────────────────────────────┐
                  │            rootfs.tar.gz               │
                  │   (deployable container / wsl layer)   │
                  └────────────────────────────────────────┘

The system bundles a unified C-based binary executing a **BusyBox routing pattern**: 

* Running ai (or ai backend) spins up the automated AI backend daemon.
* Running ai-tui (or ai tui) initiates the terminal user interface layer.

### Repository Layout

```text
.
├── .gitignore              # Protects tracking layers from compilation clutter
├── Makefile                # Primary automation gatekeeper for build steps
├── build.sh                # Main assembly script executing rootfs configurations
├── config/                 # Static configuration files
│   ├── hosts               # Local network routing template
│   └── resolv.conf         # System DNS initialization specs
└── overlay/                # Files dropped natively into the filesystem
    └── usr/local/bin/      # Installation path for the core 'ai' machine binary
```

### Quick Start

### 1. Prerequisites

Ensure your build host runs a Linux kernel environment with standard compiler
utilities and superuser tools installed: 

```bash
sudo apt update && sudo apt install -y build-essential wget tar
```

### 2. Prepare the Core Binary

Compile your single-target C AI application and copy the executable into the
tracking overlay folder: 

```bash
cp /path/to/compiled/ai overlay/usr/local/bin/ai
```

### 3. Build the Image Bundle

Execute the build automation. This script automatically drops temporary layers,
pulls down the verified Alpine distribution runtime, applies configurations,
and outputs the archive: 

```bash
sudo make
```

### Build Customization Matrix

The operational workflows are fully managed through the following scripts and options: 

### Build Script Configuration (build.sh)

To target a different architecture (e.g., building for an ARM64 embedded module
rather than a 64-bit server), update the target asset coordinates inside the script: 

```bash
# For 64-bit AMD/Intel architectures (Default)
ALPINE_URL="https://dl-cdn.alpinelinux.org/alpine/v3.24/releases/x86_64/alpine-minirootfs-3.24.3-x86_64.tar.gz"

# For 64-bit ARM architectures (Raspberry Pi, AWS Graviton)
# ALPINE_URL="https://dl-cdn.alpinelinux.org/alpine/v3.24/releases/aarch64/alpine-minirootfs-3.24.3-aarch64.tar.gz"
```

### Metadata Archive Layering

The archive utilizes explicit flag structures to ensure accurate cross-compilation
permission maps: 

* --numeric-owner: Mandates that file IDs map strictly to system IDs (UID 0),
  preventing tracking errors caused by the host machine's user tables.
* -p: Preserves execution and security permissions across all engine binaries.

### Deployment Options

Once compiled, you can immediately import the resulting output/rootfs.tar.gz bundle
into your target virtualization or container standard: 

### Docker Container Engine

```bash
cat output/rootfs.tar.gz | docker import - custom-ai-agent:latest
docker run -it --rm custom-ai-agent:latest ai
```

### Windows Subsystem for Linux (WSL)

```powershell
wsl --import CustomAgent DistroStoragePath\ output\rootfs.tar.gz
wsl -d CustomAgent
```
