#!/bin/bash

usage()
{
    echo "Usage: $0 [-f] [-i] [-n namespace] <port_config.ini>" >&2
    exit 2
}

force=false
index_arg=
namespace_args=()
while getopts ":fin:" option; do
    case "$option" in
        f) force=true ;;
        i) index_arg=-i ;;
        n) namespace_args=(-n "$OPTARG") ;;
        *) usage ;;
    esac
done
shift "$((OPTIND - 1))"
[ "$#" -eq 1 ] || usage

port_config=$1

if [ "$force" = false ] && [ -e "$port_config" ]; then
    if [ -n "$index_arg" ] && ! awk '
        /^#/ {
            gsub(/[[:space:]]+/, " ")
            sub(/ $/, "")
            if ($0 == "# name lanes alias index speed") indexed = 1
            next
        }

        NF != 5 { exit 1 }
        { rows++ }

        END { if (!indexed || !rows) exit 1 }
    ' "$port_config"; then
        echo "Cached port map lacks indices: $port_config (use -f to regenerate)" >&2
        exit 3
    fi

    exit 0
fi

generation_failed()
{
    echo "Failed to generate port map from CONFIG_DB: $port_config" >&2
    exit 1
}

append_rows()
{
    local rows
    rows=$(sonic-db-cli "${namespace_args[@]}" -- CONFIG_DB EVAL "$script" 0 "$@") &&
        printf '%s\n' "$rows" >> "$temp_config"
}

if ! mkdir -p -m 0755 -- "$(dirname -- "$port_config")"; then
    echo "Failed to create port map directory for $port_config" >&2
    exit 1
fi

if ! script=$(cat /usr/share/swss/port_config_dump.lua); then
    echo "Failed to read /usr/share/swss/port_config_dump.lua" >&2
    exit 1
fi

temp_config=$(mktemp -- "${port_config}.XXXXXX") || generation_failed
trap 'rm -f -- "$temp_config"' EXIT
trap 'exit 1' HUP INT TERM
status=0
header='# name lanes alias '
[ -n "$index_arg" ] && header+='index '
printf '%sspeed\n' "$header" > "$temp_config" || generation_failed
if ! error=$(append_rows "$index_arg" 2>&1); then
    if [[ "$error" != *", reason: MISSING_PORT_INDEX "* ]]; then
        echo "$error" >&2
        generation_failed
    fi

    echo "${error#*", reason: "}" >&2
    printf '# name lanes alias speed\n' > "$temp_config" || generation_failed
    append_rows || generation_failed
    status=3
fi
grep -q '^[^#[:space:]]' "$temp_config" || generation_failed
chmod 0644 -- "$temp_config" && mv -fT -- "$temp_config" "$port_config" || generation_failed
exit "$status"
