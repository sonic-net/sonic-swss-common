#!/bin/bash
set -euo pipefail

source /etc/os-release
if [ "$ID" != ubuntu ] || [ "$VERSION_ID" != 22.04 ]; then
    echo "Jammy host setup requires Ubuntu 22.04" >&2
    exit 1
fi

redis_version=6:7.2.17-1rl1~jammy1
obsolete_packages=(python3-pip lua-bitop lua-cjson liblua5.1-0)

case "${1:-}" in
    prepare)
        export DEBIAN_FRONTEND=noninteractive
        apt-get -o APT::Update::Error-Mode=any update
        apt-get install -y --no-install-recommends ca-certificates curl gnupg

        work=$(mktemp -d)
        trap 'rm -rf "$work"' EXIT
        curl -fsSL https://packages.redis.io/gpg -o "$work/redis.asc"
        gpg --batch --yes --dearmor \
            -o /usr/share/keyrings/sonic-test-redis.gpg "$work/redis.asc"
        chmod 644 /usr/share/keyrings/sonic-test-redis.gpg
        echo "deb [signed-by=/usr/share/keyrings/sonic-test-redis.gpg] https://packages.redis.io/deb jammy main" \
            > /etc/apt/sources.list.d/sonic-test-redis.list
        # Keep server and tools together, and do not let an ordinary apt install
        # select a newer, untested major release from the same feed.
        cat > /etc/apt/preferences.d/sonic-test-redis <<EOF
Package: redis redis-server redis-tools redis-sentinel
Pin: version $redis_version
Pin-Priority: 1000

Package: redis redis-server redis-tools redis-sentinel
Pin: origin packages.redis.io
Pin-Priority: -1
EOF
        apt-get -o APT::Update::Error-Mode=any update
        apt-get -o Dpkg::Options::=--force-confold install -y --no-install-recommends \
            "redis-server=$redis_version" "redis-tools=$redis_version"

        if ! command -v uv >/dev/null 2>&1; then
            curl -fsSL https://astral.sh/uv/install.sh -o "$work/install-uv.sh"
            UV_INSTALL_DIR=/usr/local/bin UV_NO_MODIFY_PATH=1 sh "$work/install-uv.sh"
        fi
        uv --version

        installed=()
        for package in "${obsolete_packages[@]}"; do
            if [ "$(dpkg-query -W -f='${db:Status-Status}' "$package" 2>/dev/null)" = installed ]; then
                installed+=("$package")
            fi
        done
        if [ "${#installed[@]}" -gt 0 ]; then
            # Never let cleanup silently remove an unrelated reverse dependency.
            plan=$(LC_ALL=C apt-get --simulate --no-auto-remove purge "${installed[@]}")
            while read -r action package rest; do
                case "$action" in
                    Remv|Purg)
                        case "${package%%:*}" in
                            python3-pip|lua-bitop|lua-cjson|liblua5.1-0) ;;
                            *)
                                echo "Refusing cleanup: apt would remove $package" >&2
                                exit 1
                                ;;
                        esac
                        ;;
                esac
            done <<< "$plan"
            apt-get --no-auto-remove purge -y "${installed[@]}"
        fi
        ;;
    verify)
        for package in redis-server redis-tools; do
            actual=$(dpkg-query -W -f='${db:Status-Status} ${Version}' "$package")
            if [ "$actual" != "installed $redis_version" ]; then
                echo "Unexpected $package: $actual; expected $redis_version" >&2
                exit 1
            fi
        done
        for package in "${obsolete_packages[@]}"; do
            if [ "$(dpkg-query -W -f='${db:Status-Status}' "$package" 2>/dev/null)" = installed ]; then
                echo "Obsolete Ubuntu host package was reintroduced: $package" >&2
                exit 1
            fi
        done
        ;;
    *)
        echo "Usage: $0 prepare|verify" >&2
        exit 1
        ;;
esac
