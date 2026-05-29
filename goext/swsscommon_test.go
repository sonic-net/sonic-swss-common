package swsscommon

import (
	"fmt"
	"os"
	"path/filepath"
	"testing"
	"time"
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

// waitForKcos polls the consumer until it has popped at least `expected`
// entries from one or more ZMQ messages, then returns the total drained.
// Per-entry semantic verification is exercised by the C++ unit tests in
// tests/zmq_state_ut.cpp; this Go test only proves the SWIG glue works.
func waitForKcos(t *testing.T, c ZmqConsumerStateTable, expected int) int {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	total := 0
	for time.Now().Before(deadline) && total < expected {
		q := NewKeyOpFieldsValuesQueue()
		c.Pops(q)
		total += int(q.Size())
		DeleteKeyOpFieldsValuesQueue(q)
		if total < expected {
			time.Sleep(50 * time.Millisecond)
		}
	}
	return total
}

func TestZmqProducerBatchedSet(t *testing.T) {
	const endpoint = "tcp://127.0.0.1:5710"
	const tableName = "ZMQ_BATCH_SET"
	const N = 5

	server := NewZmqServer(endpoint)
	defer DeleteZmqServer(server)
	client := NewZmqClient(endpoint)
	defer DeleteZmqClient(client)

	db := NewDBConnector("APPL_DB", uint(0), true)
	defer DeleteDBConnector(db)
	producer := NewZmqProducerStateTable(db, tableName, client, false)
	defer DeleteZmqProducerStateTable(producer)
	consumer := NewZmqConsumerStateTable(db, tableName, server)
	defer DeleteZmqConsumerStateTable(consumer)

	keys := NewVectorString()
	defer DeleteVectorString(keys)
	fvss := NewFieldValuePairsList()
	defer DeleteFieldValuePairsList(fvss)
	for i := 0; i < N; i++ {
		keys.Add(fmt.Sprintf("batch_set_key_%d", i))
		fvs := NewFieldValuePairs()
		fvs.Add(NewFieldValuePair("field", fmt.Sprintf("value_%d", i)))
		fvss.Add(fvs)
	}

	ZmqProducerBatchedSet(producer, keys, fvss)

	if got := waitForKcos(t, consumer, N); got != N {
		t.Errorf("consumer drained %d kfvs, want %d", got, N)
	}
}

func TestZmqProducerBatchedDel(t *testing.T) {
	const endpoint = "tcp://127.0.0.1:5711"
	const tableName = "ZMQ_BATCH_DEL"
	const N = 4

	server := NewZmqServer(endpoint)
	defer DeleteZmqServer(server)
	client := NewZmqClient(endpoint)
	defer DeleteZmqClient(client)

	db := NewDBConnector("APPL_DB", uint(0), true)
	defer DeleteDBConnector(db)
	producer := NewZmqProducerStateTable(db, tableName, client, false)
	defer DeleteZmqProducerStateTable(producer)
	consumer := NewZmqConsumerStateTable(db, tableName, server)
	defer DeleteZmqConsumerStateTable(consumer)

	keys := NewVectorString()
	defer DeleteVectorString(keys)
	for i := 0; i < N; i++ {
		keys.Add(fmt.Sprintf("batch_del_key_%d", i))
	}

	ZmqProducerBatchedDel(producer, keys)

	if got := waitForKcos(t, consumer, N); got != N {
		t.Errorf("consumer drained %d kfvs, want %d", got, N)
	}
}

func TestZmqProducerBatchedSend(t *testing.T) {
	const endpoint = "tcp://127.0.0.1:5712"
	const tableName = "ZMQ_BATCH_SEND"
	const N = 6

	server := NewZmqServer(endpoint)
	defer DeleteZmqServer(server)
	client := NewZmqClient(endpoint)
	defer DeleteZmqClient(client)

	db := NewDBConnector("APPL_DB", uint(0), true)
	defer DeleteDBConnector(db)
	producer := NewZmqProducerStateTable(db, tableName, client, false)
	defer DeleteZmqProducerStateTable(producer)
	consumer := NewZmqConsumerStateTable(db, tableName, server)
	defer DeleteZmqConsumerStateTable(consumer)

	keys := NewVectorString()
	defer DeleteVectorString(keys)
	ops := NewVectorString()
	defer DeleteVectorString(ops)
	fvss := NewFieldValuePairsList()
	defer DeleteFieldValuePairsList(fvss)
	for i := 0; i < N; i++ {
		keys.Add(fmt.Sprintf("send_key_%d", i))
		fvs := NewFieldValuePairs()
		if i%2 == 0 {
			ops.Add("SET")
			fvs.Add(NewFieldValuePair("k", fmt.Sprintf("v_%d", i)))
		} else {
			ops.Add("DEL")
		}
		fvss.Add(fvs)
	}

	ZmqProducerBatchedSend(producer, keys, ops, fvss)

	if got := waitForKcos(t, consumer, N); got != N {
		t.Errorf("consumer drained %d kfvs, want %d", got, N)
	}
}

func TestZmqProducerBatchedSetSizeMismatch(t *testing.T) {
	const endpoint = "tcp://127.0.0.1:5713"
	const tableName = "ZMQ_BATCH_MISMATCH"

	server := NewZmqServer(endpoint)
	defer DeleteZmqServer(server)
	client := NewZmqClient(endpoint)
	defer DeleteZmqClient(client)

	db := NewDBConnector("APPL_DB", uint(0), true)
	defer DeleteDBConnector(db)
	producer := NewZmqProducerStateTable(db, tableName, client, false)
	defer DeleteZmqProducerStateTable(producer)

	keys := NewVectorString()
	defer DeleteVectorString(keys)
	keys.Add("k0")
	keys.Add("k1")
	fvss := NewFieldValuePairsList()
	defer DeleteFieldValuePairsList(fvss)
	fvss.Add(NewFieldValuePairs())

	defer func() {
		if r := recover(); r == nil {
			t.Errorf("expected panic on size mismatch, got none")
		}
	}()
	ZmqProducerBatchedSet(producer, keys, fvss)
}
