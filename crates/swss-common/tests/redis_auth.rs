use std::fs;
use std::io::{BufRead, BufReader};
use std::os::unix::fs::PermissionsExt;
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::time::{SystemTime, UNIX_EPOCH};

use swss_common::{ConfigDBConnector, CxxString, DbConnector, RedisAuthProfile, SonicV2Connector};

const WRITER_USER: &str = "sonic-trusted-writer";
const WRITER_CREDENTIAL: &str = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

struct AuthRedis {
    child: Child,
    directory: PathBuf,
    socket: PathBuf,
    profiles_file: PathBuf,
}

impl AuthRedis {
    fn start() -> Self {
        assert_eq!(
            unsafe { libc::geteuid() },
            0,
            "Redis auth tests must run as root"
        );

        let suffix = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let directory = PathBuf::from(format!(
            "/tmp/swss-rust-redis-auth-{}-{suffix}",
            std::process::id()
        ));
        fs::create_dir(&directory).unwrap();
        fs::set_permissions(&directory, fs::Permissions::from_mode(0o755)).unwrap();

        let credential_file = directory.join("writer.secret");
        write_protected(&credential_file, WRITER_CREDENTIAL, 0o400);

        let acl_file = directory.join("users.acl");
        write_protected(
            &acl_file,
            &format!("user default off\nuser {WRITER_USER} on >{WRITER_CREDENTIAL} ~* &* +@all\n"),
            0o600,
        );

        let socket = directory.join("redis.sock");
        let profiles_file = directory.join("client-profiles.json");
        write_protected(
            &profiles_file,
            &format!(
                concat!(
                    "{{\"schema_version\":1,\"profiles\":{{\"local\":{{",
                    "\"username\":\"{}\",\"domain\":\"rust-test\",",
                    "\"credential_file\":\"{}\",",
                    "\"endpoints\":[{{\"transport\":\"unix\",\"path\":\"{}\"}}]",
                    "}}}}}}"
                ),
                WRITER_USER,
                credential_file.display(),
                socket.display()
            ),
            0o444,
        );

        let mut child = Command::new("redis-server")
            .args([
                "--appendonly",
                "no",
                "--save",
                "",
                "--notify-keyspace-events",
                "AKE",
                "--port",
                "0",
                "--unixsocket",
                socket.to_str().unwrap(),
                "--unixsocketperm",
                "700",
                "--aclfile",
                acl_file.to_str().unwrap(),
            ])
            .stdout(Stdio::piped())
            .spawn()
            .unwrap();

        let mut stdout = BufReader::new(child.stdout.take().unwrap());
        let mut line = String::new();
        loop {
            line.clear();
            assert_ne!(
                stdout.read_line(&mut line).unwrap(),
                0,
                "Redis exited during startup"
            );
            if line.contains("eady to accept connections") {
                break;
            }
        }

        Self {
            child,
            directory,
            socket,
            profiles_file,
        }
    }

    fn profile(&self) -> RedisAuthProfile {
        RedisAuthProfile::new(
            "local",
            Some(self.profiles_file.to_string_lossy().into_owned()),
        )
    }
}

impl Drop for AuthRedis {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
        let _ = fs::remove_dir_all(&self.directory);
    }
}

fn write_protected(path: &Path, contents: &str, mode: u32) {
    fs::write(path, contents).unwrap();
    fs::set_permissions(path, fs::Permissions::from_mode(mode)).unwrap();
}

#[test]
fn profile_connector_and_clone_authenticate() {
    let redis = AuthRedis::start();
    let profile = redis.profile();
    assert_eq!(profile.name(), "local");
    assert_eq!(
        profile.profiles_file(),
        Some(redis.profiles_file.to_str().unwrap())
    );

    let db = DbConnector::new_unix_with_profile(5, redis.socket.to_string_lossy(), 1000, profile)
        .unwrap();
    db.set("rust-auth-original", &CxxString::new("original"))
        .unwrap();

    let clone = db.clone_timeout(1000).unwrap();
    clone
        .set("rust-auth-clone", &CxxString::new("clone"))
        .unwrap();
    assert_eq!(db.get("rust-auth-clone").unwrap().unwrap(), "clone");
}

#[test]
fn profile_entrypoints_reject_a_missing_custom_profile_file() {
    let profile = RedisAuthProfile::new(
        "local",
        Some("/tmp/swss-rust-missing-auth-profiles.json".to_string()),
    );

    assert!(DbConnector::new_named_with_profile("TEST_DB", true, 1000, profile.clone()).is_err());

    let sonic = SonicV2Connector::new(true, None).unwrap();
    assert!(sonic
        .connect_with_profile("TEST_DB", false, &profile)
        .is_err());

    let config = ConfigDBConnector::new(true, None).unwrap();
    assert!(config.connect_with_profile(false, false, &profile).is_err());
}
