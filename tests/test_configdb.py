import signal
import os
import pytest
import threading
import time
from swsscommon import swsscommon

def test_config_db_listen_while_signal_received():
    """ Test performs ConfigDBConnector.listen() while signal is received,
    checks that the listen() call is interrupted and the regular KeyboardInterrupt is raised.
    """
    c=swsscommon.ConfigDBConnector()
    c.subscribe('A', lambda a: None)
    c.connect(wait_for_init=False)

    def deferred_sigint():
        time.sleep(10)
        os.kill(os.getpid(), signal.SIGINT)

    thr = threading.Thread(target=deferred_sigint)
    thr.start()

    with pytest.raises(KeyboardInterrupt):
        c.listen()

    thr.join()
