from swsscommon import swsscommon

def test_set_log_level():
    swsscommon.Logger.getInstance().setMinPrio(swsscommon.Logger.SWSS_INFO)
    level = swsscommon.Logger.getInstance().getMinPrio()
    assert level == swsscommon.Logger.SWSS_INFO

    swsscommon.Logger.getInstance().setMinPrio(swsscommon.Logger.SWSS_ERROR)
    level = swsscommon.Logger.getInstance().getMinPrio()
    assert level == swsscommon.Logger.SWSS_ERROR
