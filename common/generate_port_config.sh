#!/bin/bash

usage()
{
    echo "Usage: $0 [-f] [-i] <port_config.ini>" >&2
    exit 2
}

force=false
index_arg=
while getopts ":fi" option; do
    case "$option" in
        f) force=true ;;
        i) index_arg=-i ;;
        *) usage ;;
    esac
done
shift "$((OPTIND - 1))"
[ "$#" -eq 1 ] || usage

port_config=$1
if [ "$force" = false ] && [ -e "$port_config" ]; then
    if [ -n "$index_arg" ] && ! awk -F '\t' '/^#/ {next} NF != 5 || $4 == "" {exit 1} {rows++} END {if (!rows) exit 1}' "$port_config"; then
        echo "Cached port map lacks indices: $port_config (use -f to regenerate)" >&2
        exit 3
    fi
    exit 0
fi

generation_failed()
{
    rm -f -- "$port_config"
    echo "Failed to generate port map from CONFIG_DB: $port_config" >&2
    exit 1
}

if ! mkdir -p -m 0755 -- "$(dirname -- "$port_config")"; then
    echo "Failed to create port map directory for $port_config" >&2
    exit 1
fi

if ! script=$(cat /usr/share/swss/port_config_dump.lua); then
    echo "Failed to read /usr/share/swss/port_config_dump.lua" >&2
    exit 1
fi

status=0
header=$'# name\tlanes\talias\t'
[ -n "$index_arg" ] && header+=$'index\t'
printf '%sspeed\n' "$header" > "$port_config" || generation_failed
if ! error=$(sonic-db-cli -- CONFIG_DB EVAL "$script" 0 "$index_arg" 2>&1 >> "$port_config"); then
    echo "$error" >&2
    [[ "$error" == *", reason: MISSING_PORT_INDEX "* ]] || generation_failed
    printf '# name\tlanes\talias\tspeed\n' > "$port_config" || generation_failed
    sonic-db-cli -- CONFIG_DB EVAL "$script" 0 >> "$port_config" || generation_failed
    status=3
fi
grep -q '^[^#[:space:]]' "$port_config" || generation_failed
exit "$status"
