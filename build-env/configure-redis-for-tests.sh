#!/bin/bash
#
# Canonical redis-server configuration for the dataplane repos' tests, shared via
# the build-env cascade. sonic-swss-common owns this script; downstream consumers
# (e.g. sonic-sairedis) reference `configure-redis-for-tests.sh` from their own
# post_install and buildenv_setup resolves it from the cascaded sonic-swss-common
# bundle (see post_install.resolve_script) -- so the body lives in exactly one place.
#
# `notify-keyspace-events AKE` is required by sonic-swss-common /
# sonic-swss and is harmless for sonic-sairedis (verified by sonic-sairedis CI).
set -ex

sudo install -d -o redis -g redis -m 755 /var/run/redis
sudo sed -ri 's/^[# ]*notify-keyspace-events .*/notify-keyspace-events AKE/' /etc/redis/redis.conf
sudo sed -ri 's|^[# ]*unixsocket[[:space:]].*|unixsocket /var/run/redis/redis.sock|' /etc/redis/redis.conf
sudo sed -ri 's/^[# ]*unixsocketperm[[:space:]].*/unixsocketperm 777/' /etc/redis/redis.conf
sudo service redis-server restart
test "$(sudo redis-cli -s /var/run/redis/redis.sock PING)" = PONG
