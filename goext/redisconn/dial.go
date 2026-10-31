// Package redisconn exposes native Redis writer login to Go connection pools.
package redisconn

/*
#cgo CFLAGS: -I/usr/include/swss/c-api
#cgo LDFLAGS: -lswsscommon
#include <stdlib.h>
#include "dbconnector.h"
*/
import "C"

import (
	"context"
	"crypto/tls"
	"errors"
	"net"
	"os"
	"strconv"
	"strings"
	"time"
	"unsafe"
)

type DialFunc func(context.Context, string, string) (net.Conn, error)

func nativeError(result C.SWSSResult) error {
	if result.exception == C.SWSSException_None {
		return nil
	}
	defer C.SWSSString_free(result.message)
	defer C.SWSSString_free(result.location)
	return errors.New(C.GoString(C.SWSSStrRef_c_str(C.SWSSStrRef(unsafe.Pointer(result.message)))))
}

func milliseconds(timeout time.Duration) C.uint32_t {
	if timeout <= 0 {
		return 0
	}
	value := timeout / time.Millisecond
	if timeout%time.Millisecond != 0 {
		value++
	}
	if value > 1<<32-1 {
		value = 1<<32 - 1
	}
	return C.uint32_t(value)
}

// DialUnix returns an authenticated socket without selecting a logical database.
// The C++ implementation owns policy parsing, credential access and AUTH.
func DialUnix(ctx context.Context, path string, timeout time.Duration) (net.Conn, error) {
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	if strings.IndexByte(path, 0) >= 0 {
		return nil, errors.New("Redis socket path contains a null byte")
	}
	if deadline, ok := ctx.Deadline(); ok {
		remaining := time.Until(deadline)
		if remaining <= 0 {
			return nil, context.DeadlineExceeded
		}
		if timeout <= 0 || remaining < timeout {
			timeout = remaining
		}
	}
	nativePath := C.CString(path)
	defer C.free(unsafe.Pointer(nativePath))
	var fd C.int32_t
	if err := nativeError(C.SWSSRedisSocket_open_unix(nativePath, milliseconds(timeout), milliseconds(timeout), &fd)); err != nil {
		return nil, err
	}
	file := os.NewFile(uintptr(fd), "Redis Unix socket")
	defer file.Close()
	if err := ctx.Err(); err != nil {
		return nil, err
	}
	return net.FileConn(file)
}

// WrapDialer uses the native Unix factory and retains TCP/TLS dialing.
// Custom Unix dialers are rejected because their connection cannot be passed
// into the native factory. Custom TCP dialers are preserved.
func WrapDialer(existing DialFunc, timeout time.Duration, config *tls.Config) DialFunc {
	custom := existing != nil
	if timeout == 0 {
		timeout = 5 * time.Second
	}
	if timeout < 0 {
		timeout = 0
	}
	if existing == nil {
		dialer := &net.Dialer{Timeout: timeout, KeepAlive: 5 * time.Minute}
		existing = dialer.DialContext
		if config != nil {
			existing = (&tls.Dialer{NetDialer: dialer, Config: config}).DialContext
		}
	}
	return func(ctx context.Context, network, address string) (net.Conn, error) {
		if network == "unix" {
			if custom || config != nil {
				return nil, errors.New("Native Redis Unix dialing does not accept a custom dialer or TLS")
			}
			return DialUnix(ctx, address, timeout)
		}
		if network == "tcp" || network == "tcp4" || network == "tcp6" {
			host, portString, err := net.SplitHostPort(address)
			if err != nil {
				return nil, err
			}
			port, err := strconv.Atoi(portString)
			if err != nil || port < 1 || port > 65535 {
				return nil, errors.New("Invalid Redis TCP port")
			}
			if strings.IndexByte(host, 0) >= 0 {
				return nil, errors.New("Redis host contains a null byte")
			}
			nativeHost := C.CString(host)
			err = nativeError(C.SWSSRedisSocket_check_tcp(nativeHost, C.int32_t(port)))
			C.free(unsafe.Pointer(nativeHost))
			if err != nil {
				return nil, err
			}
		}
		return existing(ctx, network, address)
	}
}
