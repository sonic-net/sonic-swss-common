use std::{
    collections::HashMap,
    fs::{self, remove_file},
    io::{BufRead, BufReader},
    iter,
    process::{Child, Command, Stdio},
    sync::Mutex,
};

use lazy_static::lazy_static;
use std::sync::Arc;

use rand::{random, Rng};

use swss_common::*;

lazy_static! {
    static ref CONFIG_DB: Mutex<Option<Arc<Redis>>> = Mutex::new(None);
}

pub struct Redis {
    pub proc: Child,
    pub sock: String,
}

impl Redis {
    /// Start a Redis instance with a random unix socket. Multiple instances can be started.
    /// It is mutually exclusive with start_config_db().
    pub fn start() -> Self {
        sonic_db_config_init_for_test();
        Redis::start_with_sock(random_unix_sock())
    }

    /// Start a Redis with config_db. Only one instance can be started at a time.
    /// It is mutually exclusive with start().
    pub fn start_config_db() -> Arc<Redis> {
        CONFIG_DB
            .lock()
            .unwrap()
            .get_or_insert_with(|| {
                let sock_str = random_unix_sock();
                config_db_config_init_for_test(&sock_str);
                Arc::new(Redis::start_with_sock(sock_str))
            })
            .clone()
    }

    fn start_with_sock(sock: String) -> Self {
        #[rustfmt::skip]
        #[allow(clippy::zombie_processes)]
        let mut child = Command::new("timeout")
            .args([
                "--signal=KILL",
                "15s",
                "redis-server",
                "--appendonly", "no",
                "--save", "",
                "--notify-keyspace-events", "AKE",
                "--port", "0",
                "--unixsocket", &sock,
            ])
            .stdout(Stdio::piped())
            .spawn()
            .unwrap();
        let mut stdout = BufReader::new(child.stdout.take().unwrap());
        let mut buf = String::new();
        loop {
            buf.clear();
            if stdout.read_line(&mut buf).unwrap() == 0 {
                panic!("Redis didn't start");
            }
            // Some redis version capitalize "Ready", others have lowercase "ready" :P
            if buf.contains("eady to accept connections") {
                break Self { proc: child, sock };
            }
        }
    }

    pub fn db_connector(&self) -> DbConnector {
        DbConnector::new_unix(0, &self.sock, 0).unwrap()
    }
}

impl Drop for Redis {
    fn drop(&mut self) {
        Command::new("kill")
            .args(["-s", "TERM", &self.proc.id().to_string()])
            .status()
            .unwrap();
        self.proc.wait().unwrap();
    }
}

pub struct Defer<F: FnOnce()>(Option<F>);

impl<F: FnOnce()> Drop for Defer<F> {
    fn drop(&mut self) {
        self.0.take().unwrap()()
    }
}

const DB_CONFIG_JSON: &str = r#"
        {
            "DATABASES": {
                "db name doesn't matter": {
                    "id": 0,
                    "separator": ":",
                    "instance": "redis"
                }
            }
        }
    "#;

const CONFIG_DB_REDIS_CONFIG_JSON: &str = r#"
    {
        "INSTANCES": {
            "redis": {
                "hostname": "127.0.0.1",
                "port": {port},
                "unix_socket_path": "{path}",
                "persistence_for_warm_boot": "yes"
            }
        },
        "DATABASES": {
            "APPL_DB": {
                "id": 0,
                "separator": ":",
                "instance": "redis"
            },
            "CONFIG_DB": {
                "id": 1,
                "separator": "|",
                "instance": "redis"
            },
            "STATE_DB": {
                "id": 2,
                "separator": "|",
                "instance": "redis"
            },
            "DPU_STATE_DB": {
                "id": 3,
                "separator": "|",
                "instance": "redis"
            },
            "DPU_APPL_DB": {
                "id": 4,
                "separator": ":",
                "instance": "redis"
            }
        }
    }
"#;

const DB_GLOBAL_CONFIG_JSON: &str = r#"
    {
        "INCLUDES" : [
            {
                "include" : "db_config_test.json"
            },
            {
                "container_name" : "dpu0",
                "include" : "db_config_test.json"

            }
        ]
    }
"#;

static SONIC_DB_INITIALIZED: Mutex<bool> = Mutex::new(false);
static CONIFG_DB_INITIALIZED: Mutex<bool> = Mutex::new(false);

pub fn sonic_db_config_init_for_test() {
    // HACK
    // We need to do our own locking here because locking is not correctly implemented in
    // swss::SonicDBConfig :/
    let config_db_init = CONIFG_DB_INITIALIZED.lock().unwrap();
    // config_db and sonic_db are mutually exclusive because the lock in swss::SonicDBConfig
    assert!(!*config_db_init);

    let mut sonic_db_init = SONIC_DB_INITIALIZED.lock().unwrap();
    if !*sonic_db_init {
        fs::write("/tmp/db_config_test.json", DB_CONFIG_JSON).unwrap();
        fs::write("/tmp/db_global_config_test.json", DB_GLOBAL_CONFIG_JSON).unwrap();
        sonic_db_config_initialize("/tmp/db_config_test.json").unwrap();
        sonic_db_config_initialize_global("/tmp/db_global_config_test.json").unwrap();
        fs::remove_file("/tmp/db_config_test.json").unwrap();
        fs::remove_file("/tmp/db_global_config_test.json").unwrap();
        *sonic_db_init = true;
    }
}

fn config_db_config_init_for_test(sock_str: &str) {
    // HACK
    // We need to do our own locking here because locking is not correctly implemented in
    // swss::SonicDBConfig :/
    let sonic_db_init = SONIC_DB_INITIALIZED.lock().unwrap();
    // config_db and sonic_db are mutually exclusive because the lock in swss::SonicDBConfig
    assert!(!*sonic_db_init);

    let mut config_db_init = CONIFG_DB_INITIALIZED.lock().unwrap();
    if !*config_db_init {
        let port = random_port();
        let db_config_json = CONFIG_DB_REDIS_CONFIG_JSON
            .replace("{port}", &port.to_string())
            .replace("{path}", sock_str);
        fs::write("/tmp/db_config_test.json", db_config_json).unwrap();
        fs::write("/tmp/db_global_config_test.json", DB_GLOBAL_CONFIG_JSON).unwrap();
        sonic_db_config_initialize("/tmp/db_config_test.json").unwrap();
        sonic_db_config_initialize_global("/tmp/db_global_config_test.json").unwrap();
        fs::remove_file("/tmp/db_config_test.json").unwrap();
        fs::remove_file("/tmp/db_global_config_test.json").unwrap();
        *config_db_init = true;
    }
}

pub fn random_string() -> String {
    format!("{:0X}", random::<u64>())
}

pub fn random_cxx_string() -> CxxString {
    CxxString::new(random_string())
}

pub fn random_fvs() -> FieldValues {
    let mut field_values = HashMap::new();
    for _ in 0..rand::thread_rng().gen_range(100..1000) {
        field_values.insert(random_string(), random_cxx_string());
    }
    field_values
}

pub fn random_kfv() -> KeyOpFieldValues {
    let key = random_string();
    let operation = if random() { KeyOperation::Set } else { KeyOperation::Del };
    let field_values = if operation == KeyOperation::Set {
        // We need at least one field-value pair, otherwise swss::BinarySerializer infers that
        // the operation is DEL even if the .operation field is SET
        random_fvs()
    } else {
        HashMap::new()
    };

    KeyOpFieldValues {
        key,
        operation,
        field_values,
    }
}

pub fn random_kfvs() -> Vec<KeyOpFieldValues> {
    iter::repeat_with(random_kfv).take(100).collect()
}

pub fn random_unix_sock() -> String {
    format!("/tmp/swss-common-testing-{}.sock", random_string())
}

pub fn random_port() -> u16 {
    let mut rng = rand::thread_rng();
    rng.gen_range(1000..65535)
}

// zmq doesn't clean up its own ipc sockets, so we include a deferred operation for that
pub fn random_zmq_endpoint() -> (String, impl Drop) {
    let sock = random_unix_sock();
    let endpoint = format!("ipc://{sock}");
    (endpoint, Defer(Some(|| remove_file(sock).unwrap())))
}
