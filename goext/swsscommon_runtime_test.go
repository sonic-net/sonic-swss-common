package swsscommon

import "testing"

// TestValueTypes exercises the generated Go, cgo and C++ code without Redis.
func TestValueTypes(t *testing.T) {
	pair := NewFieldValuePair("field", "value")
	defer DeleteFieldValuePair(pair)
	if pair.GetFirst() != "field" || pair.GetSecond() != "value" {
		t.Fatalf("unexpected pair: %q=%q", pair.GetFirst(), pair.GetSecond())
	}

	values := NewFieldValuePairs()
	defer DeleteFieldValuePairs(values)
	values.Add(pair)
	if values.Size() != 1 {
		t.Fatalf("expected one pair, got %d", values.Size())
	}
	got := values.Get(0)
	if got.GetFirst() != "field" || got.GetSecond() != "value" {
		t.Fatalf("unexpected vector pair: %q=%q", got.GetFirst(), got.GetSecond())
	}
}

// TestSelect crosses into the current libswsscommon shared library without Redis.
func TestSelect(t *testing.T) {
	selector := NewSelect()
	if selector == nil || selector.Swigcptr() == 0 {
		t.Fatal("Select constructor returned no object")
	}
	defer DeleteSelect(selector)
	if !selector.IsQueueEmpty() {
		t.Fatal("a new Select has a non-empty queue")
	}
}
