//go:build gcov

package redisconn

import (
	"os"
	"testing"
)

func TestMain(m *testing.M) {
	code := m.Run()
	flushNativeCoverage()
	os.Exit(code)
}
