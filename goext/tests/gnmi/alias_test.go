package gnmi_test

import (
	"reflect"
	"testing"

	gnmi "github.com/sonic-net/sonic-gnmi/swsscommon"
	common "github.com/sonic-net/sonic-swss-common/goext"
)

// Both paths must expose the same Go types and C++ objects. A copied binding
// can compile independently but cannot satisfy this shared-owner contract.
func TestCommonBindingThroughGNMIImport(t *testing.T) {
	pair := common.NewFieldValuePair("field", "value")
	defer common.DeleteFieldValuePair(pair)
	other := gnmi.NewFieldValuePair("second", "value")
	defer gnmi.DeleteFieldValuePair(other)
	if reflect.TypeOf(pair) != reflect.TypeOf(other) {
		t.Fatal("Common and gNMI resolved to different generated Go types")
	}
	var legacy gnmi.FieldValuePair = pair
	values := gnmi.NewFieldValuePairs()
	defer gnmi.DeleteFieldValuePairs(values)
	values.Add(legacy)
	var canonical common.FieldValuePairs = values
	got := canonical.Get(0)
	if canonical.Size() != 1 || got.GetFirst() != "field" || got.GetSecond() != "value" {
		t.Fatal("gNMI's import did not use the Common-owned binding")
	}
}
