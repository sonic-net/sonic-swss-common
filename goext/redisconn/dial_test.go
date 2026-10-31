package redisconn

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"testing"
	"time"
)

func TestNativeUnixWriterAndReconnect(t *testing.T) {
	root := os.Getenv("REDIS_AUTH_TEST_DIR")
	if root == "" || os.Geteuid() != 0 {
		t.Skip("run tests/run-redis-auth-tests.sh")
	}
	policyRoot := filepath.Join(root, "policy")
	if err := os.Mkdir(policyRoot, 0755); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { os.RemoveAll(policyRoot) })
	if err := os.Mkdir(filepath.Join(policyRoot, "local-system"), 0755); err != nil {
		t.Fatal(err)
	}
	serverRoot := t.TempDir()
	socket := filepath.Join(serverRoot, "redis.sock")
	acl := filepath.Join(serverRoot, "users.acl")
	if err := os.WriteFile(acl, []byte("user default on nopass ~* +@read +select +ping\nuser go-writer on >go-password ~* +@all\n"), 0600); err != nil {
		t.Fatal(err)
	}
	command := exec.Command("redis-server", "--port", "0", "--unixsocket", socket, "--save", "", "--appendonly", "no", "--aclfile", acl)
	if err := command.Start(); err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { command.Process.Kill(); command.Wait() })
	for attempt := 0; attempt < 200; attempt++ {
		c, err := net.Dial("unix", socket)
		if err == nil {
			c.Close()
			break
		}
		if attempt == 199 {
			t.Fatal(err)
		}
		time.Sleep(10 * time.Millisecond)
	}
	policy, _ := json.Marshal(map[string]interface{}{"username": "go-writer", "unix_socket": socket, "tcp_host": "127.0.0.1", "tcp_port": 0})
	if err := os.WriteFile(filepath.Join(policyRoot, "local.json"), policy, 0644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(policyRoot, "local-system/writer.secret"), []byte("go-password"), 0400); err != nil {
		t.Fatal(err)
	}
	for attempt := 0; attempt < 2; attempt++ {
		conn, err := DialUnix(context.Background(), socket, time.Second)
		if err != nil {
			t.Fatal(err)
		}
		fmt.Fprint(conn, "SELECT 6\r\nSET binding works\r\n")
		reader := bufio.NewReader(conn)
		for i := 0; i < 2; i++ {
			line, err := reader.ReadString('\n')
			if err != nil || line != "+OK\r\n" {
				t.Fatalf("native writer reply %q: %v", line, err)
			}
		}
		conn.Close()
	}
	if err := os.WriteFile(filepath.Join(policyRoot, "local-system/writer.secret"), []byte("wrong-password"), 0400); err != nil {
		t.Fatal(err)
	}
	if conn, err := DialUnix(context.Background(), socket, time.Second); err == nil {
		conn.Close()
		t.Fatal("wrong credential must fail")
	}
}

func TestCustomUnixDialerIsRejected(t *testing.T) {
	called := false
	dial := WrapDialer(func(context.Context, string, string) (net.Conn, error) { called = true; return nil, nil }, time.Second, nil)
	if _, err := dial(context.Background(), "unix", "/missing.sock"); err == nil || called {
		t.Fatal("custom Unix dialer must not silently bypass native authentication")
	}
}
