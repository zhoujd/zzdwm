README
======

# Local AI Testing Environment (CPU Optimized)

This repository contains the deployment configuration for a localized, highly optimized AI inference testing stack. It bundles **Ollama** alongside the **Open WebUI** interface. 

The architecture is specifically configured for **single-user testing on high-core Linux servers (100+ cores)**, restricting processing to a single NUMA node cluster to eliminate thread synchronization delays and cache thrashing.

---

## Quick Start Deployment

### 1. Initialize Host Storage Paths
Before launching the containers, create the host directory and configure loose permissions so Ollama's internal container root user can read and write model blobs directly.

```bash
sudo mkdir -p /opt/tool/ollama
sudo chmod 777 /opt/tool/ollama  # Ensures Docker's internal root user can write models
```

### 2. Create the Configuration File
Save the following configuration block as `docker-compose.yml` in your deployment directory:

```yaml
version: '3.8'

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
      - webui_data:/app/backend/data
    environment:
      - OLLAMA_BASE_URL=http://ollama:11434 # Uses secure internal Docker routing
    depends_on:
      - ollama

volumes:
  webui_data:
```

### 3. Launch the Stack
Initialize and run the services in background detached mode:

```bash
docker compose up -d
```

---

## 📥 Pre-loading Testing Models

To ensure a smooth first-time login experience, pre-download the leading quantized CPU-friendly models directly to your host storage location using the following execution hooks:

| Model Type | Best Testing Model | Download Command |
| :--- | :--- | :--- |
| **Logic & Reasoning** | DeepSeek R1 (8B) | `docker exec -it ollama_cpu_core ollama pull deepseek-r1:8b` |
| **Code Generation** | Qwen 2.5 Coder (7B) | `docker exec -it ollama_cpu_core ollama pull qwen2.5-coder:7b` |
| **Balanced Chat** | Llama 3.1 (8B) | `docker exec -it ollama_cpu_core ollama pull llama3.1` |

---

## 🌐 Accessing the Interface

1. Open your web browser and navigate to: `http://<your-server-ip>:3000`
2. Click **Sign Up** to establish your local administrator credentials. *(All account metrics remain entirely offline on your hardware)*.
3. Select your pre-loaded model from the drop-down menu at the top center of the UI and begin testing.

---

## 🛠️ Performance Tuning (Troubleshooting)

If you experience textual stuttering or slow tokens-per-second output during prompt processing, apply these localized adjustments:

* **Verify Thread Lock:** Ensure the container is honoring the restriction layout. You can inspect the live hardware load across your server via `htop` or `top`. Only cores 0 through 15 should peak during generations.
* **Reduce Core Mapping:** If memory bandwidth latency limits your throughput, downscale the compute footprint. Run `docker compose down`, change `cpuset` to `"0-7"`, change `OLLAMA_NUM_THREADS` to `8`, and run `docker compose up -d`. Smaller models frequently type faster when running completely within a smaller, unified CPU L3 cache boundary.

## 🛠️ Management & Monitoring Script

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
