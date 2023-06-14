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

func TestProducerStateTable(t *testing.T) {
	db := NewDBConnector("APPL_DB", uint(0), true)
	pt := NewProducerStateTable(db, "TEST_TABLE")
	tbl := NewTable(db, "_TEST_TABLE")
	vec := NewFieldValuePairs()
	key := "aaa"
	pair := NewFieldValuePair("a", "b")
	vec.Add(pair)
	pt.Set(key, vec, "SET", "")
	fvs := NewFieldValuePairs()
	ret := tbl.Get("aaa", fvs)
	if ret != true {
		t.Errorf("Get table failed")
	}
	fv := fvs.Get(0)
	if fv.GetFirst() != "a" {
		t.Errorf("Wronge fv first %v", fv.GetFirst())
	}
	if fv.GetSecond() != "b" {
		t.Errorf("Wronge fv second: %v", fv.GetSecond())
	}
}

