package swsscommon

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

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

func TestRuntimeLibraries(t *testing.T) {
	contents, err := os.ReadFile("/proc/self/maps")
	if err != nil {
		t.Fatal(err)
	}
	loaded := make(map[string]map[string]bool)
	for _, prefix := range []string{"libswsscommon.so.", "libhiredis.so.", "libyang.so."} {
		loaded[prefix] = make(map[string]bool)
	}
	for _, line := range strings.Split(string(contents), "\n") {
		start := strings.IndexByte(line, '/')
		if start < 0 {
			continue
		}
		path := strings.TrimSuffix(line[start:], " (deleted)")
		for prefix, matches := range loaded {
			if !strings.HasPrefix(filepath.Base(path), prefix) {
				continue
			}
			resolved, err := filepath.EvalSymlinks(path)
			if err != nil {
				t.Fatal(err)
			}
			matches[resolved] = true
		}
	}

	expected := os.Getenv("SWSS_EXPECTED_LIBRARY")
	if expected == "" || len(loaded["libswsscommon.so."]) != 1 || !loaded["libswsscommon.so."][expected] {
		t.Fatalf("expected current library %q, loaded %v", expected, loaded["libswsscommon.so."])
	}
	runtimeRoot := os.Getenv("SWSS_RUNTIME_ROOT")
	if runtimeRoot == "" {
		t.Fatal("SWSS_RUNTIME_ROOT is required")
	}
	mode := os.Getenv("SWSS_YANG_MODE")
	if mode != "enabled" && mode != "disabled" {
		t.Fatalf("unknown YANG mode %q", mode)
	}
	for _, prefix := range []string{"libhiredis.so.", "libyang.so."} {
		matches := loaded[prefix]
		if prefix == "libyang.so." && mode == "disabled" {
			if len(matches) != 0 {
				t.Fatalf("no-YANG consumer loaded libyang: %v", matches)
			}
			continue
		}
		if len(matches) != 1 {
			t.Fatalf("expected one %s library, loaded %v", prefix, matches)
		}
		for path := range matches {
			relative, err := filepath.Rel(runtimeRoot, path)
			if err != nil || relative == ".." || strings.HasPrefix(relative, ".."+string(filepath.Separator)) {
				t.Fatalf("library is outside the declared runtime: %s", path)
			}
		}
	}
}
