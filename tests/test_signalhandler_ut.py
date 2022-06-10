import signal
import os
import pytest
import time
import threading

from swsscommon import swsscommon
from swsscommon.swsscommon import SonicV2Connector, SonicDBConfig
from test_redis_ut import prepare

CurrentSignalNumber = 0

def python_signal_handler(signum, stack):
    global CurrentSignalNumber
    CurrentSignalNumber = signum

def pubsub_thread():
    connector =swsscommon.ConfigDBConnector()
    connector.db_connect('CONFIG_DB')
    connector.subscribe('A', lambda a: None)
    connector.listen()

def check_signal_can_break_pubsub(signalId):
    global CurrentSignalNumber
    CurrentSignalNumber = 0
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


def test_SignalIntAndSigTerm():
    check_signal_can_break_pubsub(signal.SIGINT)
    check_signal_can_break_pubsub(signal.SIGTERM)
