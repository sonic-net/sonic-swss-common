use std::{collections::HashMap, time::Duration};
use swss_common::*;
use swss_common_testing::*;

#[test]
fn dbconnector_sync_api_basic_test() -> Result<(), Exception> {
    let redis = Redis::start();
    let db = redis.db_connector();

    drop(db.clone_timeout(10000));

    assert!(db.flush_db()?);

    let random = random_cxx_string();

    db.set("hello", &CxxString::new("hello, world!"))?;
    db.set("random", &random)?;
    assert_eq!(db.get("hello")?.unwrap(), "hello, world!");
    assert_eq!(db.get("random")?.unwrap(), random);
    assert_eq!(db.get("noexist")?, None);

    assert!(db.exists("hello")?);
    assert!(!db.exists("noexist")?);
    assert!(db.del("hello")?);
    assert!(!db.del("hello")?);
    assert!(db.del("random")?);
    assert!(!db.del("random")?);
    assert!(!db.del("noexist")?);

    db.hset("a", "hello", &CxxString::new("hello, world!"))?;
    db.hset("a", "random", &random)?;
    assert_eq!(db.hget("a", "hello")?.unwrap(), "hello, world!");
    assert_eq!(db.hget("a", "random")?.unwrap(), random);
    assert_eq!(db.hget("a", "noexist")?, None);
    assert_eq!(db.hget("noexist", "noexist")?, None);
    assert!(db.hexists("a", "hello")?);
    assert!(!db.hexists("a", "noexist")?);
    assert!(!db.hexists("noexist", "hello")?);
    assert!(db.hdel("a", "hello")?);
    assert!(!db.hdel("a", "hello")?);
    assert!(db.hdel("a", "random")?);
    assert!(!db.hdel("a", "random")?);
    assert!(!db.hdel("a", "noexist")?);
    assert!(!db.hdel("noexist", "noexist")?);
    assert!(!db.del("a")?);

    assert!(db.hgetall("a")?.is_empty());
    db.hset("a", "a", &CxxString::new("1"))?;
    db.hset("a", "b", &CxxString::new("2"))?;
    db.hset("a", "c", &CxxString::new("3"))?;
    assert_eq!(
        db.hgetall("a")?,
        HashMap::from_iter([
            ("a".into(), "1".into()),
            ("b".into(), "2".into()),
            ("c".into(), "3".into())
        ])
    );

    assert!(db.flush_db()?);

    Ok(())
}

#[test]
fn consumer_producer_state_tables_sync_api_basic_test() -> Result<(), Exception> {
    sonic_db_config_init_for_test();
    let redis = Redis::start();
    let pst = ProducerStateTable::new(redis.db_connector(), "table_a")?;
    let cst = ConsumerStateTable::new(redis.db_connector(), "table_a", None, None)?;

    assert!(cst.pops()?.is_empty());

    let mut kfvs = random_kfvs();
    for (i, kfv) in kfvs.iter().enumerate() {
        assert_eq!(pst.count()?, i as i64);
        match kfv.operation {
            KeyOperation::Set => pst.set(&kfv.key, kfv.field_values.clone())?,
            KeyOperation::Del => pst.del(&kfv.key)?,
        }
    }

    assert_eq!(cst.read_data(Duration::from_millis(2000), true)?, SelectResult::Data);
    let mut kfvs_cst = cst.pops()?;
    assert!(cst.pops()?.is_empty());

    kfvs.sort_unstable();
    kfvs_cst.sort_unstable();
    assert_eq!(kfvs_cst.len(), kfvs.len());
    assert_eq!(kfvs_cst, kfvs);

    Ok(())
}

#[test]
fn subscriber_state_table_sync_api_basic_test() -> Result<(), Exception> {
    sonic_db_config_init_for_test();
    let redis = Redis::start();
    let db = redis.db_connector();
    let sst = SubscriberStateTable::new(redis.db_connector(), "table_a", None, None)?;
    assert!(sst.pops()?.is_empty());

    db.hset("table_a:key_a", "field_a", &CxxString::new("value_a"))?;
    db.hset("table_a:key_a", "field_b", &CxxString::new("value_b"))?;
    assert_eq!(sst.read_data(Duration::from_millis(300), true)?, SelectResult::Data);
    let mut kfvs = sst.pops()?;

    // SubscriberStateTable will pick up duplicate KeyOpFieldValues' after two SETs on the same
    // key. I'm not actually sure if this is intended.
    assert_eq!(kfvs.len(), 2);
    assert_eq!(kfvs[0], kfvs[1]);

    assert!(sst.pops()?.is_empty());

    let KeyOpFieldValues {
        key,
        operation,
        field_values,
    } = kfvs.pop().unwrap();

    assert_eq!(key, "key_a");
    assert_eq!(operation, KeyOperation::Set);
    assert_eq!(
        field_values,
        HashMap::from_iter([
            ("field_a".into(), "value_a".into()),
            ("field_b".into(), "value_b".into())
        ])
    );

    Ok(())
}

#[test]
fn zmq_consumer_state_table_sync_api_basic_test() -> Result<(), Exception> {
    use SelectResult::*;

    let (endpoint, _delete) = random_zmq_endpoint();
    let mut zmqs = ZmqServer::new(&endpoint)?;
    let zmqc = ZmqClient::new(&endpoint)?;
    assert!(zmqc.is_connected()?);

    let redis = Redis::start();
    let zcst_table_a = ZmqConsumerStateTable::new(redis.db_connector(), "table_a", &mut zmqs, None, None)?;
    let zcst_table_b = ZmqConsumerStateTable::new(redis.db_connector(), "table_b", &mut zmqs, None, None)?;

    let kfvs = random_kfvs();

    zmqc.send_msg("", "table_a", kfvs.clone())?; // db name is empty because we are using DbConnector::new_unix
    assert_eq!(zcst_table_a.read_data(Duration::from_millis(1500), true)?, Data);

    zmqc.send_msg("", "table_b", kfvs.clone())?;
    assert_eq!(zcst_table_b.read_data(Duration::from_millis(1500), true)?, Data);

    let kfvs_a = zcst_table_a.pops()?;
    let kvfs_b = zcst_table_b.pops()?;
    assert_eq!(kfvs_a, kvfs_b);
    assert_eq!(kfvs, kfvs_a);

    Ok(())
}

#[test]
fn zmq_consumer_producer_state_tables_sync_api_basic_test() -> Result<(), Exception> {
    use SelectResult::*;

    let (endpoint, _delete) = random_zmq_endpoint();
    let mut zmqs = ZmqServer::new(&endpoint)?;
    let zmqc = ZmqClient::new(&endpoint)?;

    let redis = Redis::start();
    let zpst = ZmqProducerStateTable::new(redis.db_connector(), "table_a", zmqc, false)?;
    let zcst = ZmqConsumerStateTable::new(redis.db_connector(), "table_a", &mut zmqs, None, None)?;

    let kfvs = random_kfvs();
    for kfv in &kfvs {
        match kfv.operation {
            KeyOperation::Set => zpst.set(&kfv.key, kfv.field_values.clone())?,
            KeyOperation::Del => zpst.del(&kfv.key)?,
        }
    }

    let mut kfvs_seen = Vec::new();
    while kfvs_seen.len() != kfvs.len() {
        assert_eq!(zcst.read_data(Duration::from_millis(2000), true)?, Data);
        kfvs_seen.extend(zcst.pops()?);
    }
    assert_eq!(kfvs, kfvs_seen);

    Ok(())
}

// Below test covers 2 scenarios:
// 1. late connect when zmq server is started after client sending messages.
// 2. reconnect when zmq server is stopped and restarted. messages from client during
//    the time should be queued by client and resent when server is restarted.
#[test]
fn zmq_consumer_producer_state_tables_sync_api_connect_late_reconnect() -> Result<(), Exception> {
    use SelectResult::*;
    enum TestPhase {
        LateConnect,
        Reconnect,
    }
    let (endpoint, _delete) = random_zmq_endpoint();

    let zmqc = ZmqClient::new(&endpoint)?;
    let redis = Redis::start();
    let zpst = ZmqProducerStateTable::new(redis.db_connector(), "table_a", zmqc, false)?;

    for _ in [TestPhase::LateConnect, TestPhase::Reconnect] {
        let kfvs = random_kfvs();
        for kfv in &kfvs {
            match kfv.operation {
                KeyOperation::Set => zpst.set(&kfv.key, kfv.field_values.clone())?,
                KeyOperation::Del => zpst.del(&kfv.key)?,
            }
        }

        let mut zmqs = ZmqServer::new(&endpoint)?;
        let zcst = ZmqConsumerStateTable::new(redis.db_connector(), "table_a", &mut zmqs, None, None)?;
        let mut kfvs_seen = Vec::new();
        while kfvs_seen.len() != kfvs.len() {
            assert_eq!(zcst.read_data(Duration::from_millis(2000), true)?, Data);
            kfvs_seen.extend(zcst.pops()?);
        }
        assert_eq!(kfvs, kfvs_seen);
        drop(zcst);
        drop(zmqs);
    }

    Ok(())
}

#[test]
fn table_sync_api_basic_test() -> Result<(), Exception> {
    let redis = Redis::start();
    let table = Table::new(redis.db_connector(), "mytable")?;
    assert!(table.get_keys()?.is_empty());
    assert!(table.get("mykey")?.is_none());

    let fvs = random_fvs();
    table.set("mykey", fvs.clone())?;
    assert_eq!(table.get_keys()?, &["mykey"]);
    assert_eq!(table.get("mykey")?.as_ref(), Some(&fvs));

    let (field, value) = fvs.iter().next().unwrap();
    assert_eq!(table.hget("mykey", field)?.as_ref(), Some(value));
    table.hdel("mykey", field)?;
    assert_eq!(table.hget("mykey", field)?, None);

    table.hset("mykey", field, &CxxString::from("my special value"))?;
    assert_eq!(table.hget("mykey", field)?.unwrap().as_bytes(), b"my special value");

    table.del("mykey")?;
    assert!(table.get_keys()?.is_empty());
    assert!(table.get("mykey")?.is_none());

    Ok(())
}

#[test]
fn expected_exceptions() {
    DbConnector::new_tcp(0, "127.0.0.1", 1, 10000).unwrap_err();
}
