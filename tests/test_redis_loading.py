import os
import pytest
import re
import threading
import time
from swsscommon import swsscommon

TEST_TIMEOUT = 30
REDIS_RECORD_COUNT = 1000000


def wait_redis_ready():
    end_time = time.time() + TEST_TIMEOUT
    while time.time() < end_time:
        result = os.popen(r'redis-cli ping').read()
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
    os.popen(r"redis-cli save")


def test_redis_loading_exception(capfd):
    os.popen(r"redis-cli FLUSHALL")

    generate_redis_dump()

    print("restart redis service")
    os.popen(r"sudo service redis-server restart")

    # write swss log to stderr, so capfd can capture it
    swsscommon.Logger.swssOutputNotify("", "STDERR")
    swsscommon.Logger.setMinPrio(swsscommon.Logger.SWSS_NOTICE)

    print("Start test read")
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
    print("End test read")

    # wait for redis load dataset finish
    wait_redis_ready()

    print("cleanup redis data")
    os.popen(r"redis-cli FLUSHALL")
    os.popen(r"redis-cli save")
    print("end loading_dataset")

    print("end test_redis_loading_exception")
    assert loading_exception, "LOADING exception does not happen"

    expected = 'WARN:- guard: RedisReply catches system_error: command: *2\\r\\n$4\\r\\nKEYS\\r\\n$4\\r\\ntest\\r\\n, reason: LOADING Redis is loading the dataset in memory'
    captured_log = capfd.readouterr().err

    with capfd.disabled():
        print("captured syslog: {}".format(captured_log))

    assert expected in captured_log
