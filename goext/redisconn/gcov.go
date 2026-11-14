//go:build gcov

package redisconn

/*
#cgo LDFLAGS: -lgcov
void __gcov_dump(void);
*/
import "C"

// Go exits without running the native library's gcov shutdown handlers.
func flushNativeCoverage() {
	C.__gcov_dump()
}
