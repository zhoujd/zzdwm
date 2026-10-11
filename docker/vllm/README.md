# Local AI Testing Environment (vLLM CPU-Optimized Setup)

A production-grade, self-contained LLM runtime environment engineered specifically for local x86_64 CPU server architectures. This setup utilizes **vLLM** to deliver continuous batching and high-throughput concurrency, swapping out the traditional single-user sequential processing of Ollama.

---

## Infrastructure Architecture Overview

| Core Component | Implementation Tool | Internal Port | Host Port | Role Description |
| :--- | :--- | :--- | :--- | :--- |
| **Model Serving Engine** | `vllm-openai` | `8000` | `11434` | Continuous batching inference engine optimized for CPU execution graphs. |
| **User Interface** | `open-webui` | `8080` | `3000` | Local web-based chat workspace utilizing OpenAI API routing structures. |
| **Downstream App Integration** | `.codex/config.toml` | — | — | External code generation routing file pointing back to the hosting ports. |

---

## Model File Storage Schema & Lifecycle

Unlike Ollama which packs layers into single `.gguf` archives, vLLM consumes unquantized or natively quantized Hugging Face model repositories (`.safetensors`, weights, configuration parameters, and tokenizers) natively.

### 1. Host Storage Layout
All downloaded structures persist on your host machine inside `/opt/tool/vllm/`. This translates to internal container caching storage boundaries as shown below:

```text
/opt/tool/vllm/
└── hub/
    ├── models--Qwen--Qwen2.5-Coder-7B-Instruct/
    │   ├── blobs/             # Split tensor parameter files
    │   ├── snapshots/         # Hardlink references bound to unique commit IDs
    │   └── refs/              # Passive pointers tracking branch states (main)
    └── version.txt
```

### 2. Initialization Pipeline Flow
vLLM implements a automated **"Download-on-Boot"** sequence to minimize system maintenance steps:

1. **Stack Boots:** Docker spins up the engine via your configuration definitions.
2. **Local Path Validation:** The engine inspects `/opt/tool/vllm/hub/`.
3. **Optimized CDN Ingestion:** On a cache miss, the service connects to `hf-mirror.com` via the assigned `HF_ENDPOINT` proxy routing, pulling downstream assets with parallel thread pipelines.
4. **Memory Injection:** Once validated, weights are mapped straight into host CPU system RAM blocks to prepare for incoming execution prompts.

### 3. Air-Gapped / Manual File Seeding (Optional)
If you prefer to bypass runtime container downloads entirely, seed the model cache manually on your host machine using the python library directly:

```bash
HF_ENDPOINT=https://hf-mirror.com python3 -c "
from huggingface_hub import snapshot_download
snapshot_download(repo_id='Qwen/Qwen2.5-Coder-7B-Instruct', local_dir='/opt/tool/vllm/hub/models--Qwen--Qwen2.5-Coder-7B-Instruct', local_dir_use_symlinks=False)
"
```

---

## Deployment Instructions

### Prerequisites
* Docker and Docker Compose V2 must be active on your host system.
* Your CPU architecture must include Advanced Vector Extensions (**AVX-512** or **AVX2**) to guarantee performant token output generations.

### Step 1: Initialize System Infrastructure
Spin up the decoupled runtime stack using Docker Compose:

```bash
docker compose up -d
```

### Step 2: Monitor Real-Time Log Streams
Track system preparation logs to verify download progress and watch the memory loading phase:

```bash
docker logs -f vllm_cpu_coder
```

---

## Workspace Integration Schemas

### 1. Codex Workspace Engine Integration (`.codex/config.toml`)
To wire your local development editor, CLI, or agent systems straight into this vLLM service block, paste the following model footprint schema inside your project's configuration directory:

```toml
[codex]
default_model = "Qwen/Qwen2.5-Coder-7B-Instruct"
purpose = "code-generation"

[provider.openai]
base_url = "http://localhost:11434/v1"
api_key = "vllm-not-needed"
request_timeout = 90

[[models]]
name = "Qwen/Qwen2.5-Coder-7B-Instruct"
role = "default"
temperature = 0.2
top_p = 0.9
```

### 2. General User UI Workspaces
1. Open your host machine browser and go to: **`http://localhost:3000`**
2. Register a master admin credential pair (all user records remain offline inside `/opt/tool/open-webui`).
3. Select **`Qwen/Qwen2.5-Coder-7B-Instruct`** from the model dropdown selector.

---

## Performance Fine-Tuning Benchmarks

* **Physical Core Pinning (`cpuset: "0-15"`):** Hardlocked exclusively to your first physical CPU socket bounds. This prevents expensive multi-socket cross-talk penalties over the system bus.
* **Shared Memory Boost (`shm_size: '4gb'`):** Allocated for low-overhead inter-process memory maps during high-density matrix evaluations.
* **Network Acceleration (`HF_ENDPOINT`):** Forwards model requests directly through Chinese mainland edge CDNs to maximize download pipelines.
