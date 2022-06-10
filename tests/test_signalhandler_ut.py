import signal
import os
import pytest
import time
import threading

from swsscommon import swsscommon
from swsscommon.swsscommon import SonicV2Connector, SonicDBConfig
from test_redis_ut import prepare

def dummy_signal_handler(signum, stack):
    pass

def pubsub_thread():
    connector =swsscommon.ConfigDBConnector()
    connector.db_connect('CONFIG_DB')
    connector.subscribe('A', lambda a: None)
    connector.listen()

def check_signal_can_break_pubsub(signalId):
    signal.signal(signalId, dummy_signal_handler)

    test_thread = threading.Thread(target=pubsub_thread)
    test_thread.start()

    # check thread is running
    time.sleep(2)
    assert test_thread.is_alive() == True

    os.kill(os.getpid(), signalId)

    # check thread is stopped
    time.sleep(2)
    assert test_thread.is_alive() == False

def test_SignalIntAndSigTerm():
    check_signal_can_break_pubsub(signal.SIGINT)
    check_signal_can_break_pubsub(signal.SIGTERM)
