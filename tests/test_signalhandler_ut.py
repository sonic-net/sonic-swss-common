import signal
import os
import pytest
import multiprocessing
import time
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


def test_config_db_listen_while_signal_received():
    """ Test performs ConfigDBConnector.listen() while signal is received,
    checks that the listen() call is interrupted and the regular KeyboardInterrupt is raised.
    """
    c=swsscommon.ConfigDBConnector()
    c.subscribe('A', lambda a: None)
    c.connect(wait_for_init=False)
    event = multiprocessing.Event()

    def signal_handler(signum, frame):
        event.set()
        sys.exit(0)

    signal.signal(signal.SIGUSR1, signal_handler)

    def listen():
        c.listen()

    thr = multiprocessing.Process(target=listen)
    thr.start()

    time.sleep(5)
    os.kill(thr.pid, signal.SIGUSR1)

    thr.join()

    assert event.is_set()
