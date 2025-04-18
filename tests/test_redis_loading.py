import os
import time
from swsscommon import swsscommon

TEST_TIMEOUT = 10
REDIS_RECORD_COUNT = 2000000


def wait_redis_ready():
    end_time = time.time() + TEST_TIMEOUT
    while time.time() < end_time:
        result = os.popen(r'redis-cli ping').read()
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
    """ Test write warning message to syslog when redis is loading data.
    The redis loading data exception is difficult to reproduce, it's only happen when redis service starting and loading massive data.
    This test will create one million data dump and restart redis server, however on fast device the test will failed.
    Currently the test can pass on developer's devbox.
    So this test only report error message when test failed, please developer run this test manually to verify this scenario.
    """

    # cleanup redis data
    os.popen(r"redis-cli FLUSHALL")

    generate_redis_dump()

    print("restart redis service")
    os.popen(r"sudo service redis-server restart")

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
                break

    # wait for redis load dataset finish
    wait_redis_ready()

    # cleanup test data
    os.popen(r"redis-cli FLUSHALL")
    os.popen(r"redis-cli save")

    captured_log = capfd.readouterr().err
    expected = 'WARN:- guard: RedisReply catches system_error: command: *2\\r\\n$4\\r\\nKEYS\\r\\n$4\\r\\ntest\\r\\n, reason: LOADING Redis is loading the dataset in memory'
    if expected not in captured_log:
        print("Test redis loading data exception failed, could not find expected warning message from swsscommon syslog: {}".format(expected))
        
