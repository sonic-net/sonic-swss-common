import signal
import os
import pytest
from swsscommon import swsscommon
from swsscommon.swsscommon import SignalHandlerHelper

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