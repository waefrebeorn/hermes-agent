#!/bin/bash
# hermes bash completion — H01
# Source:  source <(hermes completions bash)
# Install: hermes completions bash > /etc/bash_completion.d/hermes

_hermes_completions() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local prev="${COMP_WORDS[COMP_CWORD-1]}"
    local opts="--help -h --version -v --session gateway cron --tui completions"

    # Complete subcommands
    if [[ $COMP_CWORD -eq 1 ]]; then
        COMPREPLY=($(compgen -W "$opts" -- "$cur"))
        return 0
    fi

    # Complete --session with <id>
    if [[ "$prev" == "--session" ]]; then
        # Could list sessions here via db query
        COMPREPLY=($(compgen -W "" -- "$cur"))
        return 0
    fi

    # Gateway subcommand
    if [[ "$prev" == "gateway" ]]; then
        local gw_opts="--platform --port --help"
        COMPREPLY=($(compgen -W "$gw_opts" -- "$cur"))
        return 0
    fi

    # Cron subcommand
    if [[ "${COMP_WORDS[1]}" == "cron" ]]; then
        local cron_opts="--help start stop list"
        COMPREPLY=($(compgen -W "$cron_opts" -- "$cur"))
        return 0
    fi

    COMPREPLY=()
}

complete -F _hermes_completions hermes
