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

sudo sed -i  's/notify-keyspace-events ""/notify-keyspace-events AKE/' /etc/redis/redis.conf
sudo sed -ri 's/^# unixsocket/unixsocket/'                             /etc/redis/redis.conf
sudo sed -ri 's/^unixsocketperm .../unixsocketperm 777/'              /etc/redis/redis.conf
sudo sed -ri 's/redis-server.sock/redis.sock/'                        /etc/redis/redis.conf
sudo service redis-server restart
