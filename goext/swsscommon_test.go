package swsscommon

import (
	"testing"
	"os"
	"path/filepath"
)

func TestSonicDBConfig(t *testing.T) {
	pwd, _ := os.Getwd()
	SonicDBConfigInitialize(filepath.Dir(pwd) + "/tests/redis_multi_db_ut_config/database_config.json")
}

