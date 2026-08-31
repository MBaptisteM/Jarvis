#!/bin/bash

# --- Setup Terminal Colors ---
CYAN='\033[0;36m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'
BOLD='\033[1m'

echo -e "${CYAN}${BOLD}>>> Starting the installation process...${NC}\n"

# --- 1. Create a temporary directory ---
echo -e "${CYAN}[1/3]${NC} Creating temporary workspace..."
TMP_DIR=$(mktemp -d)

# --- 2. Set cleanup trap ---
trap "rm -rf $TMP_DIR; echo -e '\n${CYAN}>>> Cleanup complete. Temporary files removed.${NC}'" EXIT

# --- 3. Clone the repository ---
echo -e "${CYAN}[2/3]${NC} Downloading the latest version..."
if ! git clone --depth 1 https://github.com/MBaptisteM/Jarvis.git "$TMP_DIR" > /dev/null 2>&1; then
    echo -e "${RED}${BOLD}Error:${NC} Failed to download the repository. Please check the URL or your connection."
    exit 1
fi

# --- 4. Run the installation script ---
echo -e "${CYAN}[3/3]${NC} Running the setup...\n"
cd "$TMP_DIR" || exit
chmod +x ./install
./install

echo -e "\n${GREEN}${BOLD}>>> Installation finished successfully!${NC}"