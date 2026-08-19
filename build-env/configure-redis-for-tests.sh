#!/bin/bash
#
# Canonical redis-server configuration for the dataplane repos' tests, shared via
# the build-env cascade. sonic-swss-common owns this script; downstream consumers
# (e.g. sonic-sairedis) reference `configure-redis-for-tests.sh` from their own
# post_install and buildenv_setup resolves it from the cascaded sonic-swss-common
# bundle (see post_install.resolve_script) -- so the body lives in exactly one place.
#
# Reproduces exactly the redis config build-template.yml applied inline before the
# unit tests. `notify-keyspace-events AKE` is required by sonic-swss-common /
# sonic-swss and is harmless for sonic-sairedis (verified by sonic-sairedis CI).
set -ex

REDIS_CONFIG=${REDIS_CONFIG:-/etc/redis/redis.conf}
SONIC_DB_CONFIG=${SONIC_DB_CONFIG:-/var/run/redis/sonic-db/database_config.json}

# libswsscommon installs its DB configuration under /var/run/redis. Restarting
# redis-server can recreate that runtime directory and remove the file, so keep
# a temporary copy and restore it after the restart.
sonic_db_config_backup=""
cleanup()
{
    [ -z "$sonic_db_config_backup" ] || rm -f "$sonic_db_config_backup"
}
trap cleanup EXIT

if sudo test -f "$SONIC_DB_CONFIG"; then
    sonic_db_config_backup=$(mktemp)
    sudo cp "$SONIC_DB_CONFIG" "$sonic_db_config_backup"
fi

sudo sed -i  's/notify-keyspace-events ""/notify-keyspace-events AKE/' "$REDIS_CONFIG"
sudo sed -ri 's/^# unixsocket/unixsocket/'                             "$REDIS_CONFIG"
sudo sed -ri 's/^unixsocketperm .../unixsocketperm 777/'              "$REDIS_CONFIG"
sudo sed -ri 's/redis-server.sock/redis.sock/'                         "$REDIS_CONFIG"
sudo service redis-server restart

if [ -n "$sonic_db_config_backup" ]; then
    sudo install -d -m 755 "$(dirname "$SONIC_DB_CONFIG")"
    sudo install -m 644 "$sonic_db_config_backup" "$SONIC_DB_CONFIG"
fi
