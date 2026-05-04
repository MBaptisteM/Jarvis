_jarvis_completions() {
    local cur commands

    cur="${COMP_WORDS[COMP_CWORD]}"

    if [ -d "$HOME/.local/lib/jarvis/cmd" ]; then
        cmd_dir="$HOME/.local/lib/jarvis/cmd"
    else
        cmd_dir="/usr/local/lib/jarvis/cmd"
    fi

    if [ -d "$cmd_dir" ]; then
        commands=$(find "$cmd_dir" -maxdepth 1 -type f -printf "%f\n" 2>/dev/null)
    else
        commands=""
    fi

    COMPREPLY=( $(compgen -W "${commands}" -- ${cur}) )
}

complete -F _jarvis_completions jarvis