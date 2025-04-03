import os
import pytest
import re
import subprocess
import threading
import time
from swsscommon import swsscommon

TEST_TIMEOUT = 30
REDIS_RECORD_COUNT = 1000000


def start_redis():
    # limit redis using 1% CPU because to increase redis loading time on fast environment
    print("start redis with CPU limit")
    os.system(r"sudo cpulimit --limit 1 -- /usr/bin/redis-server  /etc/redis/redis.conf")


def stop_redis():
    print("stop redis with CPU limit")
    os.system("sudo pkill redis-server")


def wait_redis_ready():
    end_time = time.time() + TEST_TIMEOUT
    while time.time() < end_time:
        result = subprocess.getoutput("redis-cli ping")
        print("ping result: {}".format(result))
        if "PONG" in result:
            break
        time.sleep(1)


def generate_redis_dump():
    print("generate redis dump")
    # generate a large redis dataset to reproduce LOADING exception
    db = swsscommon.SonicV2Connector(use_unix_socket_path=True)
    db.connect("TEST_DB")
    id = REDIS_RECORD_COUNT
    while id > 0:
        id -= 1
        db.set("TEST_DB", "record:{}".format(id), "field", "value{}".format(id))

    # create dump file
    os.system("redis-cli save")


@pytest.fixture
def loading_dataset():
    wait_redis_ready()

    os.system("redis-cli FLUSHALL")
    os.system("redis-cli save")
    generate_redis_dump()

    wait_redis_ready()

    print("stop redis service")
    os.system("sudo service redis-server stop")

    # start redis server in a thread, so test can run during redis loading data
    thread = threading.Thread(target=start_redis)
    thread.start()

    yield

    thread.join()
    stop_redis()

    # cleanup and restore redis service
    print("start redis service")
    os.system("sudo service redis-server start")

    # wait for redis load dataset finish
    wait_redis_ready()

    print("cleanup redis data")
    os.system("redis-cli FLUSHALL")
    os.system("redis-cli save")


def test_redis_loading_exception(loading_dataset, capfd):
    # write swss log to stderr, so capfd can capture it
    swsscommon.Logger.swssOutputNotify("", "STDERR")
    swsscommon.Logger.setMinPrio(swsscommon.Logger.SWSS_NOTICE)

    end_time = time.time() + TEST_TIMEOUT
    loading_exception = False
    while time.time() < end_time:
        try:
            db = swsscommon.DBConnector("CONFIG_DB", 0, True)
            db.keys("test")
        except Exception as e:
            error = "{}".format(e)
            if "LOADING" in error:
                loading_exception = True
                break

    assert loading_exception, "LOADING exception does not happen"

    pattern = r'.*WARN.*, reason: LOADING Redis is loading the dataset in memory.*'
    captured_log = capfd.readouterr().err

    with capfd.disabled():
        print("captured syslog: {}".format(captured_log))

    assert re.match(pattern, captured_log)
