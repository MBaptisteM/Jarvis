jarvis() {
    if [[ ${1-} == go || ${1-} == g ]]; then
        case ${2-} in
            --help|-h|help)
                command jarvis "$@"
                return $?
                ;;
        esac

        local target previous_directory=$PWD
        target=$(command jarvis "$@") || return $?
        [[ -n $target ]] || return 1
        cd -- "$target" || return $?
        OLDPWD=$previous_directory
    else
        command jarvis "$@"
    fi
}
