_jarvis_completions() {
    local cur commands

    # Mot en cours de saisie
    cur="${COMP_WORDS[COMP_CWORD]}"

    # Chemin vers les commandes
    local cmd_dir="/usr/local/lib/jarvis"

    # Si tu testes en local (avant install), utilise :
    # cmd_dir="./commands"

    # Liste des commandes
    if [ -d "$cmd_dir" ]; then
        commands=$(ls "$cmd_dir")
    else
        commands=""
    fi

    # Générer les suggestions
    COMPREPLY=( $(compgen -W "${commands}" -- ${cur}) )
}

# Associer à la commande mytool
complete -F _jarvis_completions jarvis