#![cfg(feature = "async")]
use paste::paste;
use std::{collections::HashMap, time::Duration};
use swss_common::*;
use swss_common_testing::*;

async fn timeout<F: std::future::Future>(timeout_ms: u32, fut: F) -> F::Output {
    tokio::time::timeout(Duration::from_millis(timeout_ms.into()), fut)
        .await
        .expect("timed out")
}

// This macro verifies that async test functions are Send. tokio::test is a bit misleading because
// a tokio runtime's root future can be !Send. This takes a test function and defines two tests from
// it - one that is the root future (Send not required), and one that uses a type assertion to check
// that the tested future is Send.
macro_rules! define_tokio_test_fns {
    ($f:ident) => {
        paste! {
            #[tokio::test]
            async fn [< $f _ >]() {
                $f().await.unwrap();
            }

            fn [< _assert_ $f _is_send >]() {
                fn assert_is_send<T: Send>(_: T) {}
                assert_is_send($f());
            }
        }
    };
}

define_tokio_test_fns!(dbconnector_async_api_basic_test);
async fn dbconnector_async_api_basic_test() -> Result<(), Exception> {
    let redis = Redis::start();
    let mut db = redis.db_connector();

    drop(db.clone_timeout_async(10000).await?);

    assert!(db.flush_db_async().await?);

    let random = random_cxx_string();

    db.set_async("hello", &CxxString::new("hello, world!")).await?;
    db.set_async("random", &random).await?;
    assert_eq!(db.get_async("hello").await?.unwrap(), "hello, world!");
    assert_eq!(db.get_async("random").await?.unwrap(), random);
    assert_eq!(db.get_async("noexist").await?, None);

    assert!(db.exists_async("hello").await?);
    assert!(!db.exists_async("noexist").await?);
    assert!(db.del_async("hello").await?);
    assert!(!db.del_async("hello").await?);
    assert!(db.del_async("random").await?);
    assert!(!db.del_async("random").await?);
    assert!(!db.del_async("noexist").await?);

    db.hset_async("a", "hello", &CxxString::new("hello, world!")).await?;
    db.hset_async("a", "random", &random).await?;
    assert_eq!(db.hget_async("a", "hello").await?.unwrap(), "hello, world!");
    assert_eq!(db.hget_async("a", "random").await?.unwrap(), random);
    assert_eq!(db.hget_async("a", "noexist").await?, None);
    assert_eq!(db.hget_async("noexist", "noexist").await?, None);
    assert!(db.hexists_async("a", "hello").await?);
    assert!(!db.hexists_async("a", "noexist").await?);
    assert!(!db.hexists_async("noexist", "hello").await?);
    assert!(db.hdel_async("a", "hello").await?);
    assert!(!db.hdel_async("a", "hello").await?);
    assert!(db.hdel_async("a", "random").await?);
    assert!(!db.hdel_async("a", "random").await?);
    assert!(!db.hdel_async("a", "noexist").await?);
    assert!(!db.hdel_async("noexist", "noexist").await?);
    assert!(!db.del_async("a").await?);

    assert!(db.hgetall_async("a").await?.is_empty());
    db.hset_async("a", "a", &CxxString::new("1")).await?;
    db.hset_async("a", "b", &CxxString::new("2")).await?;
    db.hset_async("a", "c", &CxxString::new("3")).await?;
    assert_eq!(
        db.hgetall_async("a").await?,
        HashMap::from_iter([
            ("a".into(), "1".into()),
            ("b".into(), "2".into()),
            ("c".into(), "3".into())
        ])
    );

    assert!(db.flush_db_async().await?);

    Ok(())
}

define_tokio_test_fns!(consumer_producer_state_tables_async_api_basic_test);
async fn consumer_producer_state_tables_async_api_basic_test() -> Result<(), Exception> {
    let redis = Redis::start();
    let mut pst = ProducerStateTable::new(redis.db_connector(), "table_a")?;
    let mut cst = ConsumerStateTable::new(redis.db_connector(), "table_a", None, None)?;

    assert!(cst.pops()?.is_empty());

    let mut kfvs = random_kfvs();
    for (i, kfv) in kfvs.iter().enumerate() {
        assert_eq!(pst.count()?, i as i64);
        match kfv.operation {
            KeyOperation::Set => pst.set_async(&kfv.key, kfv.field_values.clone()).await?,
            KeyOperation::Del => pst.del_async(&kfv.key).await?,
        }
    }

    timeout(2000, cst.read_data_async()).await.unwrap();
    let mut kfvs_cst = cst.pops()?;
    assert!(cst.pops()?.is_empty());

    kfvs.sort_unstable();
    kfvs_cst.sort_unstable();
    assert_eq!(kfvs_cst.len(), kfvs.len());
    assert_eq!(kfvs_cst, kfvs);

    Ok(())
}

define_tokio_test_fns!(subscriber_state_table_async_api_basic_test);
async fn subscriber_state_table_async_api_basic_test() -> Result<(), Exception> {
    let redis = Redis::start();
    let mut db = redis.db_connector();
    let mut sst = SubscriberStateTable::new(redis.db_connector(), "table_a", None, None)?;
    assert!(sst.pops()?.is_empty());

    db.hset_async("table_a:key_a", "field_a", &CxxString::new("value_a"))
        .await?;
    db.hset_async("table_a:key_a", "field_b", &CxxString::new("value_b"))
        .await?;
    timeout(300, sst.read_data_async()).await.unwrap();
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

define_tokio_test_fns!(zmq_consumer_producer_state_table_async_api_basic_test);
async fn zmq_consumer_producer_state_table_async_api_basic_test() -> Result<(), Exception> {
    let (endpoint, _delete) = random_zmq_endpoint();
    let mut zmqs = ZmqServer::new(&endpoint)?;
    let zmqc = ZmqClient::new(&endpoint)?;

    let redis = Redis::start();
    let mut zpst = ZmqProducerStateTable::new(redis.db_connector(), "table_a", zmqc, false)?;
    let mut zcst = ZmqConsumerStateTable::new(redis.db_connector(), "table_a", &mut zmqs, None, None)?;

    let kfvs = random_kfvs();
    for kfv in &kfvs {
        match kfv.operation {
            KeyOperation::Set => zpst.set_async(&kfv.key, kfv.field_values.clone()).await?,
            KeyOperation::Del => zpst.del_async(&kfv.key).await?,
        }
    }

    let mut kfvs_seen = Vec::new();
    while kfvs_seen.len() != kfvs.len() {
        timeout(2000, zcst.read_data_async()).await.unwrap();
        kfvs_seen.extend(zcst.pops()?);
    }
    assert_eq!(kfvs, kfvs_seen);

    Ok(())
}

define_tokio_test_fns!(table_async_api_basic_test);
async fn table_async_api_basic_test() -> Result<(), Exception> {
    let redis = Redis::start();
    let mut table = Table::new_async(redis.db_connector(), "mytable").await?;
    assert!(table.get_keys_async().await?.is_empty());
    assert!(table.get_async("mykey").await?.is_none());

    let fvs = random_fvs();
    table.set_async("mykey", fvs.clone()).await?;
    assert_eq!(table.get_keys_async().await?, &["mykey"]);
    assert_eq!(table.get_async("mykey").await?.as_ref(), Some(&fvs));

    let (field, value) = fvs.iter().next().unwrap();
    assert_eq!(table.hget_async("mykey", field).await?.as_ref(), Some(value));
    table.hdel_async("mykey", field).await?;
    assert_eq!(table.hget_async("mykey", field).await?, None);

    table
        .hset_async("mykey", field, &CxxString::from("my special value"))
        .await?;
    assert_eq!(
        table.hget_async("mykey", field).await?.unwrap().as_bytes(),
        b"my special value"
    );

    table.del_async("mykey").await?;
    assert!(table.get_keys_async().await?.is_empty());
    assert!(table.get_async("mykey").await?.is_none());

    Ok(())
}
