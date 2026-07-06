#!/bin/bash
#
# Configure the system redis-server for sonic-swss-common's tests.
# Applied via the `configure-redis-for-tests` post_install entry in
# build-env/packages/base.yaml (scopes: build, test).
#
# This reproduces exactly the redis config that build-template.yml applied
# inline before running the unit tests.
set -ex

sudo sed -i  's/notify-keyspace-events ""/notify-keyspace-events AKE/' /etc/redis/redis.conf
sudo sed -ri 's/^# unixsocket/unixsocket/'                             /etc/redis/redis.conf
sudo sed -ri 's/^unixsocketperm .../unixsocketperm 777/'              /etc/redis/redis.conf
sudo sed -ri 's/redis-server.sock/redis.sock/'                        /etc/redis/redis.conf
sudo service redis-server restart
