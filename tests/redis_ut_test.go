package redis_test

import (
	"fmt"
	"os"
	"os/exec"
	"testing"

	swsscommon "github.com/Azure/sonic-swss-common"
)

func Test_Table(t *testing.T) {
	db := swsscommon.NewDBConnector(0, "localhost", 6379, uint(0))
	defer swsscommon.DeleteDBConnector(db)
	tbl := swsscommon.NewTable(db, "abc")
	defer swsscommon.DeleteTable(tbl)
	fvps := swsscommon.NewFieldValuePairs()
	defer swsscommon.DeleteFieldValuePairs(fvps)
	fvp1 := swsscommon.NewStringPair("a", "b")
	defer swsscommon.DeleteStringPair(fvp1)
	fvp2 := swsscommon.NewStringPair("c", "d")
	defer swsscommon.DeleteStringPair(fvp2)

	fvps.Add(fvp1)
	fvps.Add(fvp2)

	tbl.Set("aaa", fvps)

	keys := swsscommon.NewVectorString()
	defer swsscommon.DeleteVectorString(keys)
	tbl.GetKeys(keys)
	if keys.Size() != 1 {
		t.Error("Wrong keys size: ", keys.Size())
	}

	if keys.Get(0) != "aaa" {
		t.Error("Wrong keys: ", keys.Get(0))
	}

	vpsr := swsscommon.NewFieldValuePairs()
	defer swsscommon.DeleteFieldValuePairs(vpsr)
	ret := tbl.Get("aaa", vpsr)
	if ret != true {
		t.Error("table get failed")
	}

	if vpsr.Size() != 2 {
		t.Error("Wrong FieldValuePairs size: ", vpsr.Size())
	}

	fp0 := vpsr.Get(0)
	if fp0.GetFirst() != "a" || fp0.GetSecond() != "b" {
		t.Error("Wrong First FieldValuePair ")
	}

	fp1 := vpsr.Get(1)
	if fp1.GetFirst() != "c" || fp1.GetSecond() != "d" {
		t.Error("Wrong First FieldValuePair ")
	}
	tbl.Del("aaa")
}

func Test_ProducerStateTable(t *testing.T) {
	db := swsscommon.NewDBConnector(0, "localhost", 6379, uint(0))
	defer swsscommon.DeleteDBConnector(db)
	ps := swsscommon.NewProducerStateTable(db, "abc")
	defer swsscommon.DeleteProducerStateTable(ps)
	cs := swsscommon.NewConsumerStateTable(db, "abc")
	defer swsscommon.DeleteConsumerStateTable(cs)

	fvps := swsscommon.NewFieldValuePairs()
	defer swsscommon.DeleteFieldValuePairs(fvps)
	fvp1 := swsscommon.NewStringPair("a", "b")
	defer swsscommon.DeleteStringPair(fvp1)
	fvps.Add(fvp1)

	ps.Set("aaa", fvps)

	vpsr := swsscommon.NewFieldValuePairs()
	defer swsscommon.DeleteFieldValuePairs(vpsr)
	// Reusing FieldValuePair for KeyOpTuple as std::pair<std::string, std::string>
	ko := swsscommon.NewStringPair()
	defer swsscommon.DeleteStringPair(ko)
	//var key, op string
	cs.Pop(ko, vpsr)
	if ko.GetFirst() != "aaa" {
		t.Error("Wrong key: ", ko.GetFirst())
	}

	if ko.GetSecond() != "SET" {
		t.Error("Wrong op: ", ko.GetSecond())
	}

	if vpsr.Size() != 1 {
		t.Error("Wrong FieldValuePairs size: ", vpsr.Size())
	}

	fp0 := vpsr.Get(0)
	if fp0.GetFirst() != "a" || fp0.GetSecond() != "b" {
		t.Error("Wrong FieldValuePair ")
	}
}

func Test_SubscriberStateTable(t *testing.T) {
	os.Setenv("PATH", "/usr/bin:/sbin:/bin")
	cmd := exec.Command("redis-cli", "config", "set", "notify-keyspace-events", "KEA")
	output, err := cmd.Output()
	if err != nil {
		os.Stderr.WriteString(err.Error())
		fmt.Println(string(output))
	}

	db := swsscommon.NewDBConnector(0, "localhost", 6379, uint(0))
	defer swsscommon.DeleteDBConnector(db)
	tbl := swsscommon.NewTable(db, "testsst", "|")
	defer swsscommon.DeleteTable(tbl)

	sel := swsscommon.NewSelect()
	defer swsscommon.DeleteSelect(sel)

	cst := swsscommon.NewSubscriberStateTable(db, "testsst")
	defer swsscommon.DeleteSubscriberStateTable(cst)

	sel.AddSelectable(cst.SwigGetSelectable())

	fvs := swsscommon.NewFieldValuePairs()
	defer swsscommon.DeleteFieldValuePairs(fvs)
	fvp1 := swsscommon.NewStringPair("a", "b")
	defer swsscommon.DeleteStringPair(fvp1)
	fvs.Add(fvp1)

	tbl.Set("aaa", fvs)

	//const timout = 1000
	// s := swsscommon.NewRedisSelect()
	//defer swsscommon.DeleteRedisSelect(s)
	fd := []int{0}
	var timeout uint = 1000
	//ret := sel.Xselect(s.SwigGetSelectable(), fd, timeout)
	ret := sel.Xselect(fd, timeout)

	if ret != swsscommon.SelectOBJECT {
		t.Error("Expecting : ", swsscommon.SelectOBJECT)
	}

	vpsr := swsscommon.NewFieldValuePairs()
	defer swsscommon.DeleteFieldValuePairs(vpsr)
	// Reusing FieldValuePair for KeyOpTuple as std::pair<std::string, std::string>
	ko := swsscommon.NewStringPair()
	defer swsscommon.DeleteStringPair(ko)
	cst.Pop(ko, vpsr)

	if ko.GetFirst() != "aaa" {
		t.Error("Wrong key: ", ko.GetFirst())
	}

	if ko.GetSecond() != "SET" {
		t.Error("Wrong op: ", ko.GetSecond())
	}

	if vpsr.Size() != 1 {
		t.Error("Wrong FieldValuePairs size: ", vpsr.Size())
	}

	fp0 := vpsr.Get(0)
	if fp0.GetFirst() != "a" || fp0.GetSecond() != "b" {
		t.Error("Wrong FieldValuePair ")
	}
}
