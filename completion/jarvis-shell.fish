function jarvis
    if test (count $argv) -gt 0
        if test "$argv[1]" = go; or test "$argv[1]" = g
            if test (count $argv) -gt 1
                if test "$argv[2]" = --help; or test "$argv[2]" = -h; or test "$argv[2]" = help
                    command jarvis $argv
                    return $status
                end
            end

            set -l previous_directory $PWD
            set -l target (command jarvis $argv)
            or return $status
            if test -z "$target"
                return 1
            end
            cd -- "$target"
            or return $status
            set -gx OLDPWD "$previous_directory"
        else
            command jarvis $argv
        end
    else
        command jarvis
    end
end
