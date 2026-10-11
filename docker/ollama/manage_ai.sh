#!/bin/bash

# --- CONFIGURATION (Matches README.md / Docker Setup) ---
CONTAINER_NAME="ollama_cpu_core"
CPU_SET="0-15"
LOG_DIR="/opt/tool/ollama/models/blobs"

# --- COLOR PROMPTS ---
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0;0m' # No Color

clear
while true; do
    echo -e "${CYAN}======================================================${NC}"
    echo -e "${GREEN}      MicroEMACS Style Local AI Tester Dashboard      ${NC}"
    echo -e "${CYAN}======================================================${NC}"
    echo -e "1) ${YELLOW}[Status]${NC} Check Stack Container Status"
    echo -e "2) ${YELLOW}[Monitor]${NC} Watch Live CPU Thread Allocations (Cores 0-15)"
    echo -e "3) ${YELLOW}[Speed]${NC} Benchmark Local Inference Token Speed"
    echo -e "4) ${YELLOW}[Storage]${NC} List Downloaded Model Blobs (/opt/tool)"
    echo -e "5) ${YELLOW}[Logs]${NC} View Real-time Ollama Service Logs"
    echo -e "6) ${RED}[Exit]${NC} Quit Dashboard"
    echo -e "${CYAN}------------------------------------------------------${NC}"
    read -p "Select an action [1-6]: " choice

    case $choice in
        1)
            echo -e "\n${GREEN}--- Container Lifecycle Status ---${NC}"
            docker ps --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}" | grep -E "ollama|webui" || echo -e "${RED}Containers are offline.${NC}"
            echo ""
            read -p "Press [Enter] to return to menu..."
            clear
            ;;
        2)
            echo -e "\n${GREEN}--- Monitoring Core Clustering ---${NC}"
            echo -e "${YELLOW}Notice:${NC} During generation, only physical cores 0-15 should see utilization spikes."
            if command -v htop &> /dev/null; then
                # Launches htop directly focused on thread distribution metrics
                htop
            else
                echo -e "${YELLOW}htop not found. Falling back to standard top core diagnostics (Press Q to exit)...${NC}"
                sleep 2
                top -1
            fi
            clear
            ;;
        3)
            echo -e "\n${GREEN}--- Measuring Local Inference Response Speed ---${NC}"
            read -p "Enter a prompt to test (Default: 'Why use S-expressions?'): " user_prompt
            if [ -z "$user_prompt" ]; then
                user_prompt="Why are S-expressions optimized for resource-constrained systems?"
            fi
            
            # Grabs the first pulled model dynamically to run the benchmark test
            active_model=$(docker exec -i $CONTAINER_NAME ollama list | awk 'NR>1 {print $1}' | head -n 1)
            
            if [ -z "$active_model" ]; then
                echo -e "${RED}Error: No models found inside Ollama storage folder. Pull a model first.${NC}"
            else
                echo -e "${CYAN}Targeting active model:${NC} $active_model"
                echo -e "${CYAN}Executing stream profile (Press Ctrl+C to abort testing metrics mid-way)...${NC}\n"
                
                # Executes inference and prints structural timing marks
                time docker exec -it $CONTAINER_NAME ollama run "$active_model" "$user_prompt"
            fi
            echo ""
            read -p "Press [Enter] to return to menu..."
            clear
            ;;
        4)
            echo -e "\n${GREEN}--- Hard Drive Model Storage Profile ---${NC}"
            if [ -d "$LOG_DIR" ]; then
                echo -e "${CYAN}Target Path:${NC} /opt/tool/ollama"
                echo -e "${CYAN}Total storage footprint occupied by AI structures:${NC}"
                du -sh /opt/tool/ollama
                echo -e "\n${CYAN}Individual binary model blobs currently cached:${NC}"
                ls -lh "$LOG_DIR"
            else
                echo -e "${RED}Error: /opt/tool/ollama directory layout does not exist or container has not initialized models yet.${NC}"
            fi
            echo ""
            read -p "Press [Enter] to return to menu..."
            clear
            ;;
        5)
            echo -e "\n${GREEN}--- Real-time System Framework Logs (Press Ctrl+C to exit) ---${NC}"
            docker logs -f $CONTAINER_NAME --tail 50
            clear
            ;;
        6)
            echo -e "\n${GREEN}Exiting dashboard. Local servers remain running in background layers.${NC}"
            exit 0
            ;;
        *)
            echo -e "\n${RED}Invalid selection.${NC}"
            sleep 1
            clear
            ;;
    esac
done
