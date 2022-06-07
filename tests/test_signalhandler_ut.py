import signal
import os
import pytest
import time
import threading

from swsscommon import swsscommon
from swsscommon.swsscommon import SonicV2Connector
from swsscommon.signal import SignalHandlerHelper, RegisterSignal

CurrentSignalNumber = 0

def python_signal_handler(signum, stack):
    global CurrentSignalNumber
    CurrentSignalNumber = signum

def dummy_signal_handler(signum, stack):
    pass

def test_SignalHandler():
    RegisterSignal(signal.SIGUSR1, python_signal_handler)

    # Register SIGUSER1
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
    # register python signal handler so SIGUSER1 will not break test
    signal.signal(signal.SIGUSR1, dummy_signal_handler)
    os.kill(os.getpid(), signal.SIGUSR1)
    happened = SignalHandlerHelper.checkSignal(signal.SIGUSR1)
    assert happened == False

def pubsub_thread():
    connector =swsscommon.ConfigDBConnector()
    connector.subscribe('A', lambda a: None)
    connector.connect()
    connector.listen()

def check_signal_can_break_pubsub(signalId):
    global CurrentSignalNumber
    CurrentSignalNumber = 0
    SignalHandlerHelper.resetSignal(signalId)
    RegisterSignal(signalId, python_signal_handler)

    test_thread = threading.Thread(target=pubsub_thread)
    test_thread.start()

    # check thread is running
    time.sleep(2)
    assert test_thread.is_alive() == True

    os.kill(os.getpid(), signalId)

    # check thread is stopped
    time.sleep(2)
    assert test_thread.is_alive() == False

    # check 
    assert CurrentSignalNumber == signalId

def test_SignalIntAndSigTerm():
    check_signal_can_break_pubsub(signal.SIGINT)
    check_signal_can_break_pubsub(signal.SIGTERM)
