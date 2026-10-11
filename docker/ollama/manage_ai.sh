#!/bin/bash
# Master Management Script for Local AI Environments

VENV_PATH="$HOME/.venv/emacs/bin"
BASE_DIR=$(dirname "$(readlink -f "$0")")

function show_usage() {
    echo "Usage: ./manage_ai.sh [command]"
    echo "Commands:"
    echo "  start-headless   Start CPU-optimized Ollama engine without UI"
    echo "  start-webui      Start full stack including Open-WebUI"
    echo "  stop             Shut down whichever stack is currently active"
    echo "  status           Display container status and resource mapping"
    echo "  pull-coder       Accelerated download of Qwen 2.5 Coder 7B via mirror"
}

case "$1" in
    start-headless)
        echo "⚡ Initializing core headless deployment..."
        cd "$BASE_DIR/NoWebUI" && docker compose up -d
        ;;
    start-webui)
        echo "🖥️ Initializing full user-interface deployment..."
        cd "$BASE_DIR/WebUI" && docker compose up -d
        ;;
    stop)
        echo "🛑 Terminating all active local AI services..."
        cd "$BASE_DIR/NoWebUI" && docker compose down
        cd "$BASE_DIR/WebUI" && docker compose down
        ;;
    status)
        echo "📊 Displaying resource layout boundaries:"
        docker ps --filter "name=ollama" --filter "name=webui" --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"
        ;;
    pull-coder)
        echo "🚀 Running multi-threaded pipeline download via hf-mirror.com..."
        HF_ENDPOINT=https://hf-mirror.com $VENV_PATH/python3 -c "
from huggingface_hub import snapshot_download
snapshot_download(
    repo_id='Qwen/Qwen2.5-Coder-7B-Instruct',
    local_dir='/opt/tool/vllm/hub/models--Qwen--Qwen2.5-Coder-7B-Instruct',
    local_dir_use_symlinks=False
)
"
        ;;
    *)
        show_usage
        exit 1
        ;;
esac
