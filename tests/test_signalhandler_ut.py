import signal
import os
import pytest
import time
import threading

from swsscommon import swsscommon
from swsscommon.swsscommon import SonicV2Connector, SonicDBConfig

CurrentSignalNumber = 0
StopThread = False

def python_signal_handler(signum, stack):
    global CurrentSignalNumber
    CurrentSignalNumber = signum

def pubsub_thread():
    global StopThread
    connector =swsscommon.ConfigDBConnector()
    connector.db_connect('CONFIG_DB')
    connector.subscribe('A', lambda a: None)

    # listen_message method should not break Python signal handler
    pubsub = connector.get_redis_client(connector.db_name).pubsub()
    pubsub.psubscribe("__keyspace@{}__:*".format(connector.get_dbid(connector.db_name)))
    GET_MESSAGE_INTERVAL = 10.0;
    while not StopThread:
        pubsub.listen_message(GET_MESSAGE_INTERVAL)

def check_signal_can_break_pubsub(signalId):
    global CurrentSignalNumber
    CurrentSignalNumber = 0
    global StopThread
    StopThread = False
    signal.signal(signalId, python_signal_handler)

    test_thread = threading.Thread(target=pubsub_thread)
    test_thread.start()

    # check thread is running
    time.sleep(2)
    assert test_thread.is_alive() == True

    os.kill(os.getpid(), signalId)

    # signal will not break listen() method
    time.sleep(2)
    assert test_thread.is_alive() == True

    # python signal handler will receive signal
    assert CurrentSignalNumber == signalId
    
    # stop pubsub thread
    StopThread = True
    time.sleep(2)
    assert test_thread.is_alive() == False

def test_SignalIntAndSigTerm():
    check_signal_can_break_pubsub(signal.SIGINT)
    check_signal_can_break_pubsub(signal.SIGTERM)
