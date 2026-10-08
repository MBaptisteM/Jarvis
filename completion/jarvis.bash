_jarvis_completions() {
    local cur cmd_dir command commands

    cur="${COMP_WORDS[COMP_CWORD]}"
    cmd_dir="$HOME/.local/lib/jarvis/cmd"

    if [ -d "$cmd_dir" ]; then
        commands=""
        for command in "$cmd_dir"/*; do
            if [ -f "$command" ]; then
                commands+="${command##*/} "
            fi
        done
    else
        commands=""
    fi

    COMPREPLY=( $(compgen -W "${commands}" -- "$cur") )
}

complete -F _jarvis_completions jarvis