import signal
import os
import pytest
import time
import threading

from swsscommon import swsscommon
from swsscommon.swsscommon import SignalHandlerHelper, SonicV2Connector

def dummy_signal_handler(signum, stack):
    # ignore signal so UT will not break
    pass

def test_SignalHandler():
    signal.signal(signal.SIGUSR1, dummy_signal_handler)

    # Register SIGUSER1
    SignalHandlerHelper.registerSignalHandler(signal.SIGUSR1)
    happened = SignalHandlerHelper.checkSignal(signal.SIGUSR1)
    assert happened == False

    # trigger SIGUSER manually
    os.kill(os.getpid(), signal.SIGUSR1)
    happened = SignalHandlerHelper.checkSignal(signal.SIGUSR1)
    assert happened == True
    
    # Reset signal
    SignalHandlerHelper.resetSignal(signal.SIGUSR1)
    happened = SignalHandlerHelper.checkSignal(signal.SIGUSR1)
    assert happened == False
    
    # un-register signal handler
    SignalHandlerHelper.restoreSignalHandler(signal.SIGUSR1)
    os.kill(os.getpid(), signal.SIGUSR1)
    happened = SignalHandlerHelper.checkSignal(signal.SIGUSR1)
    assert happened == False

def pubsub_thread():
    connector =swsscommon.ConfigDBConnector()
    connector.subscribe('A', lambda a: None)
    connector.connect()
    connector.listen()

def check_signal_can_break_pubsub(signalId):
    signal.signal(signal.SIGUSR1, dummy_signal_handler)
    SignalHandlerHelper.resetSignal(signalId)

    test_thread = threading.Thread(target=pubsub_thread)
    test_thread.start()

    # check thread is running
    time.sleep(2)
    assert test_thread.is_alive() == True

    # send SIGTERM and SIGINT will break test case, so send SIGUSR1 to trigger signal status check inside PubSub
    SignalHandlerHelper.onSignal(signalId)
    os.kill(os.getpid(), signal.SIGUSR1)

    # check thread is stopped
    time.sleep(2)
    assert test_thread.is_alive() == False
    SignalHandlerHelper.resetSignal(signalId)

def test_SignalIntAndSigTerm():
    check_signal_can_break_pubsub(signal.SIGINT)
    check_signal_can_break_pubsub(signal.SIGTERM)