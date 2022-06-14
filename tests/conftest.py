from os import path
import pytest
from swsscommon.swsscommon import SonicDBConfig


existing_file = path.dirname(__file__) + "/redis_multi_db_ut_config/database_config.json"

@pytest.fixture(scope="session", autouse=True)
def prepare(request):
    SonicDBConfig.initialize(existing_file)

