package hiredis

// #include <hiredis/hiredis.h>
import "C"

// allocateReader crosses the same external header/library boundary that the
// Common bindings use, without requiring a Redis server.
func allocateReader() bool {
	reader := C.redisReaderCreate()
	if reader == nil {
		return false
	}
	C.redisReaderFree(reader)
	return true
}
