import signal
import os
import pytest
import multiprocessing
import time
from swsscommon import swsscommon

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
