# Headless Local AI Testing Environment (CPU Optimized)

A lightweight, high-performance LLM runtime environment engineered specifically for local x86_64 CPU architectures. This configuration removes all heavy web-frontend dependencies, running **Ollama** as a decoupled background engine to serve your developer environment directly.

Headless Mode Execution Boundaries
* **Primary Target Context**: Low-overhead IDE extensions, `Emacs`, `.codex/config.toml` automation loops.
* **Network Strategy**: Complete bypass of GHCR image pulling limitations.
* **Connectivity Endpoint**: Accessible at `http://localhost:11434`.

---

## Infrastructure Architecture

| Component | Implementation | Host Port | Role Description |
| :--- | :--- | :--- | :--- |
| **Model Engine** | `ollama/ollama:latest` | `11434` | Headless inference server handling model weights and API traffic. |
| **Workspace Routing** | `.codex/config.toml` | — | Direct local socket connection mapping your IDE to the model. |

---

## Deployment Blueprint

### Prerequisites
* Docker and Docker Compose V2 installed.
* Host CPU supporting **AVX2** or **AVX-512** vector extensions.

### Step 1: Spin Up the Headless Core
Save your stripped-down configuration file as `docker-compose.yml` and initialize the background container:

```bash
docker compose up -d
```

### Step 2: Seed the Coding Model
Since the core engine is running, pull the **Qwen 2.5 Coder (7B)** model directly into the persistent storage volume. Choose **one** of the following methods depending on your network setup:

#### Method A: Direct Pull (If using a system network proxy)
```bash
docker exec -it ollama_cpu_core ollama pull qwen2.5-coder:7b
```

#### Method B: Accelerated Python Seed (For China Users via `hf-mirror.com`)
Run this script inside your host terminal to pull the model files at full speed via your active Python virtual environment:
```bash
HF_ENDPOINT=https://hf-mirror.com /home/zach/.venv/emacs/bin/python3 -c "
from huggingface_hub import snapshot_download
snapshot_download(
    repo_id='Qwen/Qwen2.5-Coder-7B-Instruct',
    local_dir='/opt/tool/vllm/hub/models--Qwen--Qwen2.5-Coder-7B-Instruct',
    local_dir_use_symlinks=False
)
"
```

---

## Editor & Tools Integration Layout

### 1. Codex Configuration (`.codex/config.toml`)
To wire your local terminal development tools, autonomous agents, or scripts straight into this engine, use the following local socket mapping footprint:

```toml
[codex]
default_model = "qwen2.5-coder:7b"
purpose = "code-generation"

[provider.ollama]
base_url = "http://localhost:11434"
request_timeout = 60

[[models]]
name = "qwen2.5-coder:7b"
role = "default"
temperature = 0.2
top_p = 0.9
```

### 2. Connectivity Sanity Check
Verify that your headless runtime server is alive and accepting incoming generation prompts from your IDE extensions by running this command on your host:

```bash
curl http://localhost:11434/api/tags
```
*Expected output: A JSON payload listing your loaded model metadata blocks.*

---

## Core Performance Tuning Flags

* **`cpuset: "0-15"`**: Hardlocks execution threads exclusively to physical cores `0` through `15`. This prevents high-latency multi-socket bus cross-talk and scheduling jitter.
* **`OLLAMA_NUM_THREADS=16`**: Configures the underlying compute matrix math engine to match your exact physical core allocation allocation.
* **`OLLAMA_NUM_PARALLEL=1` & `OLLAMA_MAX_LOADED_MODELS=1`**: Locks runtime focus onto a single model instance. This guarantees that your CPU L3 cache fields are completely dedicated to giving you peak token generation speeds.

---

## The Ultimate 2-Step Fix

### Step 1: Open an SSH Tunnel

```
ssh -L 11434:localhost:11434 user@remotehost
```

### Step 2: Launch Codex Normally

```
cat config.toml
# Force the open-source flag and provider bypass
oss = true
local_provider = "ollama"
ollama_mode = "responses"  # Bypasses the Chat vs Responses prompt

[codex]
default_model = "qwen2.5-coder:7b"
purpose = "code-generation"

[provider.ollama]
base_url = "http://localhost:11434"
request_timeout = 60

[projects."/home/zach/.codex"]
trust_level = "trusted"

codex --oss -m "qwen2.5-coder:7b" -c max_tokens=2048 -c stream_mode=raw
codex --oss -m "qwen2.5-coder:7b" -c max_tokens=2048 -c stream=true
```
