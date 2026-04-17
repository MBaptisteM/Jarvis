#!/bin/bash

set -e

echo "Installing Jarvis..."

make


if [ "$EUID" -eq 0 ]; then
    echo "Installing as root..."

    BIN_PATH="/usr/local/bin"
    LIB_PATH="/usr/local/lib/jarvis"
    COMP_PATH="/etc/bash_completion.d"
    SUDO=""

elif command -v sudo >/dev/null 2>&1; then
    echo "Installing with sudo..."

    BIN_PATH="/usr/local/bin"
    LIB_PATH="/usr/local/lib/jarvis"
    COMP_PATH="/etc/bash_completion.d"
    SUDO="sudo"

else
    echo "Installing locally..."

    BIN_PATH="$HOME/.local/bin"
    LIB_PATH="$HOME/.local/lib/jarvis"
    COMP_PATH="$HOME/.local/share/bash-completion/completions"
    SUDO=""
fi


$SUDO mkdir -p "$BIN_PATH"
$SUDO mkdir -p "$LIB_PATH"
$SUDO mkdir -p "$COMP_PATH"


$SUDO cp bin/jarvis "$BIN_PATH/"
$SUDO cp -r commands/* "$LIB_PATH/"
$SUDO cp completion/jarvis.bash "$COMP_PATH/jarvis"


$SUDO chmod +x "$BIN_PATH/jarvis"
$SUDO chmod +x "$LIB_PATH"/*

echo "Installation complete !"


if [[ "$BIN_PATH" == "$HOME/.local/bin" ]]; then
    if [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
        echo ""
        echo "Jarvis may not work if you do not Add this to your ~/.bashrc :"
        echo 'export PATH="$HOME/.local/bin:$PATH"'
    fi
else
    echo "You can use: jarvis"
fi