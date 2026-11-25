package hiredis

import "testing"

func TestReaderAllocation(t *testing.T) {
	if !allocateReader() {
		t.Fatal("hiredis failed to allocate a reader")
	}
}
