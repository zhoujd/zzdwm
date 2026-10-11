# Local AI Testing Environment (CPU Optimized)

This repository contains the deployment configuration for a localized, highly optimized AI inference testing stack. It bundles **Ollama** alongside the **Open WebUI** interface.

The architecture is specifically configured for **single-user testing on high-core Linux servers (100+ cores)**, restricting processing to a single NUMA node cluster to eliminate thread synchronization delays and cache thrashing.

Interface Workspace Mode Execution Boundaries
* **Primary Target Context**: Collaborative multi-user testing sessions, visual chat prompts, fine-tuning evaluations.
* **Network Strategy**: Employs reverse-proxy acceleration for swift container loading.
* **Connectivity Endpoint**: Workspace browser panel accessible at `http://localhost:3000`.

---

## Quick Start Deployment

### 1. Initialize Host Storage Paths
Create the centralized tool directories under `/opt/tool/` and configure permissions so the internal container root users can safely execute disk read/write loops.

```bash
sudo mkdir -p /opt/tool/ollama
sudo mkdir -p /opt/tool/open-webui
sudo chmod 777 /opt/tool/ollama /opt/tool/open-webui
```

### 2. Create the Configuration File
Save the following configuration block as `docker-compose.yml` in your deployment directory:

```yaml
# Project: Local AI Testing Environment (CPU Optimized)

services:
  ollama:
    image: ollama/ollama:latest
    container_name: ollama_cpu_core
    restart: unless-stopped
    volumes:
      - /opt/tool/ollama:/root/.ollama
    cpuset: "0-15" # Restricts to a single physical core socket block to avoid cross-talk
    environment:
      - OLLAMA_HOST=0.0.0.0:11434
      - OLLAMA_NUM_THREADS=16       # Optimized performance sweet-spot for CPU math
      - OLLAMA_NUM_PARALLEL=1       # Hardlocked for single-user peak throughput
      - OLLAMA_MAX_LOADED_MODELS=1  # Maximizes CPU L3 cache focus

  open-webui:
    image: ghcr.io/open-webui/open-webui:main
    container_name: open_webui_interface
    restart: unless-stopped
    ports:
      - "3000:8080" # Exposed User Interface port
    volumes:
      - /opt/tool/open-webui:/app/backend/data
    environment:
      - OLLAMA_BASE_URL=http://ollama:11434 # Uses secure internal Docker routing
    depends_on:
      - ollama
```

### 3. Launch the Stack
Initialize and run the services in background detached mode:

```bash
docker compose up -d
```

---

## Pre-loading Testing Models
To ensure a smooth first-time login experience, pre-download the leading quantized CPU-friendly models directly to your host storage location using the following execution hooks:

| Model Type | Best Testing Model | Download Command |
| :--- | :--- | :--- |
| **Logic & Reasoning** | DeepSeek R1 (8B) | `docker exec -it ollama_cpu_core ollama pull deepseek-r1:8b` |
| **Code Generation** | Qwen 2.5 Coder (7B) | `docker exec -it ollama_cpu_core ollama pull qwen2.5-coder:7b` |
| **Balanced Chat** | Llama 3.1 (8B) | `docker exec -it ollama_cpu_core ollama pull llama3.1` |

---

## Accessing the Interface
1. Open your web browser and navigate to: `http://<your-server-ip>:3000`
2. Click **Sign Up** to establish your local administrator credentials. *(All account metrics remain entirely offline on your hardware)*.
3. Select your pre-loaded model from the drop-down menu at the top center of the UI and begin testing.

---

## ️Performance Tuning (Troubleshooting)
If you experience textual stuttering or slow tokens-per-second output during prompt processing, apply these localized adjustments:

* **Verify Thread Lock:** Ensure the container is honoring the restriction layout. You can inspect the live hardware load across your server via `htop` or `top`. Only cores 0 through 15 should peak during generations.
* **Reduce Core Mapping:** If memory bandwidth latency limits your throughput, downscale the compute footprint. Run `docker compose down`, change `cpuset` to `"0-7"`, change `OLLAMA_NUM_THREADS` to `8`, and run `docker compose up -d`. Smaller models frequently type faster when running completely within a smaller, unified CPU L3 cache boundary.

---

## Management & Monitoring Script
The workspace includes a diagnostic utility script (`manage_ai.sh`) to simplify local administration.

### Operational Features
1. **CPU Tracker:** Isolates process observation to verify that only physical cores `0-15` hit execution load, confirming that zero NUMA latency boundaries are breached.
2. **Speed Profiler:** Runs a live `time`-tracked text terminal generator to check model throughput speeds directly in raw stdout without using WebUI overhead.
3. **Storage Inspector:** Tracks physical storage footprint consumption inside `/opt/tool/ollama`.

### Execution

```bash
chmod +x manage_ai.sh
./manage_ai.sh
```

---

## Codex Configuration

### Example .codex/config.toml

```
[codex]
# Primary engine designation
default_model = "qwen2.5-coder:7b"
purpose = "code-generation"

[provider.ollama]
# Internal Docker network endpoint or localhost endpoint
base_url = "http://localhost:11434"
request_timeout = 60

[[models]]
name = "qwen2.5-coder:7b"
role = "default"
temperature = 0.2
top_p = 0.9

[[models]]
name = "deepseek-r1:8b"
role = "reasoning"
temperature = 0.6

[[models]]
name = "llama3.1"
role = "chat"
temperature = 0.7
```

### Context Verification
- Local Machine Access: If your system parsing .codex/config.toml runs directly on your host operating system, keep base_url as http://localhost:11434.
- Dockerized Codex Access: If your Codex instance is running inside another container on the same Docker network, change the base_url line to "http://ollama:11434".

---

## HF-Mirror Model Seeding (No Proxy Alternative)

### Fetch the model
If you cannot route out via a proxy network pipeline, you can bypass the registry altogether. You can fetch the model directly via the high-speed domestic mirror Hugging Face Mirror (hf-mirror.com) and initialize it locally.
For instance, to manually download the Qwen2.5-Coder-7B model:
1. Download the GGUF asset file directly through your browser or an optimized download manager from the domestic mirror site.
2. Save the .gguf file to your persistent host directory at /opt/tool/ollama/.
3. Create a plain text configuration file named Modelfile inside that folder:dockerfile
```
FROM /root/.ollama/qwen2.5-coder-7b.gguf
```
4. Build the model into your local registry cache instantly without any network download overhead:
```
docker exec -it ollama_cpu_core ollama create qwen2.5-coder:7b -f /root/.ollama/Modelfile
```

### Model Mapping Registry

| Model Type | Target Model | Official Hugging Face Model ID | Target File Name (Q4_K_M) |
| :--- | :--- | :--- | :--- |
| **Logic & Reasoning** | DeepSeek R1 (8B) | [`unsloth/DeepSeek-R1-Distill-Llama-8B-GGUF`](https://huggingface.co/unsloth/DeepSeek-R1-Distill-Llama-8B-GGUF) | `DeepSeek-R1-Distill-Llama-8B-Q4_K_M.gguf` |
| **Code Generation** | Qwen 2.5 Coder (7B) | [`Qwen/Qwen2.5-Coder-7B-Instruct-GGUF`](https://huggingface.co/Qwen/Qwen2.5-Coder-7B-Instruct-GGUF) | `qwen2.5-coder-7b-instruct-q4_k_m.gguf` |
| **Balanced Chat** | Llama 3.1 (8B) | [`lmstudio-community/Meta-Llama-3.1-8B-Instruct-GGUF`](https://hf-mirror.com/unsloth/Llama-3.1-8B-Instruct-GGUF) | `Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf` |
