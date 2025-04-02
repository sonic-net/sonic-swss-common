import os
import pytest
import re
import subprocess
import threading
import time
from swsscommon import swsscommon

TEST_TIMEOUT = 30
REDIS_RECORD_COUNT = 3000000


def start_redis():
    process = subprocess.Popen("sudo service redis-server start", shell=True, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
    stdout, stderr = process.communicate()
    print("start redis result: stdout={} stderr={}".format(stdout.decode(), stderr.decode()))


def wait_redis_ready():
    end_time = time.time() + TEST_TIMEOUT
    while time.time() < end_time:
        result = subprocess.getoutput("redis-cli ping")
        print("ping result: {}".format(result))
        if "PONG" in result:
            break
        time.sleep(1)


@pytest.fixture
def loading_dataset():
    wait_redis_ready()

    # generate a large redis dataset to reproduce LOADING exception
    db = swsscommon.SonicV2Connector(use_unix_socket_path=True)
    db.connect("TEST_DB")
    id = REDIS_RECORD_COUNT
    while id > 0:
        id -= 1
        db.set("TEST_DB", "record:{}".format(id), "field", "value{}".format(id))

    # create dump file
    os.system("redis-cli save")

    wait_redis_ready()

    os.system("sudo service redis-server stop")

    # start redis server in a thread to reload dataset
    thread = threading.Thread(target=start_redis)
    thread.start()

    yield

    thread.join()

    # wait for redis load dataset finish
    wait_redis_ready()

    # cleanup
    os.system("redis-cli FLUSHALL")


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
