#!/bin/bash
# slermes bash completion
_slermes_completions() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local prev="${COMP_WORDS[COMP_CWORD-1]}"
    local opts="--help -h --version -v init doctor completions gateway cron"

    if [[ $COMP_CWORD -eq 1 ]]; then
        COMPREPLY=($(compgen -W "$opts" -- "$cur"))
        return 0
    fi
    COMPREPLY=()
}
complete -F _slermes_completions slermes
