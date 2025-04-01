import os
import pytest
import subprocess
import threading
import time
from swsscommon import swsscommon

TEST_TIMEOUT = 10
REDIS_RECORD_COUNT = 1000000

def restart_redis():
    process = subprocess.Popen("sudo service redis-server restart", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    print("Command result: stdout={} stderr={}".format(stdout.decode(), stderr.decode()))

@pytest.fixture
def loading_dataset():
    # generate a large redis dataset to reproduce LOADING exception
    db = swsscommon.SonicV2Connector(use_unix_socket_path=True)
    db.connect("TEST_DB")
    id = REDIS_RECORD_COUNT
    while id > 0:
        id -= 1
        db.set("TEST_DB", "record:{}".format(id), "field", "value{}".format(id))

    # create dump file
    os.system("redis-cli save")

    # restart redis server in a thread to reload dataset
    thread = threading.Thread(target=restart_redis)
    thread.start()

    yield

    thread.join()

    # wait for redis load dataset finish
    end_time = time.time() + TEST_TIMEOUT
    while time.time() < end_time:
        result=subprocess.getoutput("redis-cli ping")
        print("ping result: {}".format(result))
        if "PONG" in result:
            break
        time.sleep(1)

    # cleanup
    os.system("redis-cli FLUSHALL")

def test_redis_loading_exception(loading_dataset):
    end_time = time.time() + TEST_TIMEOUT
    loading_exception = False
    while time.time() < end_time:
        try:
            db = swsscommon.DBConnector("CONFIG_DB", 0, True)
            db.keys("test")
        except Exception as e:
            error = "{}".format(e)
            print("Exception: {}".format(error))
            if "LOADING" in error:
                loading_exception = True
                break
    
    assert loading_exception, "LOADINg exception does not happen"
