# Copilot Instructions for sonic-swss-common

## Project Overview

sonic-swss-common is the shared foundation library for SONiC's Switch State Service. It provides database communication abstractions (Redis client wrappers, producer/consumer tables), netlink wrappers, logging, and common utility functions used by virtually every other SONiC C++ and Python component. This is the most fundamental library in the SONiC ecosystem.

## Architecture

```
sonic-swss-common/
├── common/              # Core C++ library source
│   ├── dbconnector.*    # Redis database connection management
│   ├── table.*          # Table abstraction (base for producers/consumers)
│   ├── producerstatetable.*  # Producer side of pub/sub tables
│   ├── consumerstatetable.*  # Consumer side of pub/sub tables
│   ├── subscriberstatetable.* # Subscriber pattern for table changes
│   ├── select.*         # Event multiplexing (select/epoll wrapper)
│   ├── netlink.*        # Netlink socket wrappers
│   ├── netdispatcher.*  # Netlink message dispatcher
│   ├── schema.h         # Database schema definitions (table names, DB IDs)
│   ├── logger.*         # Logging infrastructure
│   ├── json.*           # JSON utilities
│   └── ...
├── pyext/               # Python SWIG bindings (python-swsscommon)
├── goext/               # Go language bindings
├── crates/              # Rust bindings
├── sonic-db-cli/        # Command-line tool for database access
├── tests/               # Google Test unit tests
├── debian/              # Debian packaging
└── gen_cfg_schema.py    # Configuration schema generator
```

### Key Concepts
- **Multi-database architecture**: SONiC uses multiple Redis databases (CONFIG_DB, APP_DB, ASIC_DB, STATE_DB, COUNTERS_DB, etc.)
- **Producer/Consumer tables**: Primary IPC mechanism between SONiC daemons
- **Select framework**: Event-driven I/O multiplexing for monitoring multiple table changes
- **Schema definitions**: `schema.h` defines all table names and database identifiers

## Language & Style

- **Primary languages**: C++ (core library), Python (SWIG bindings, tests), Go (go bindings)
- **C++ standard**: C++14/17
- **Indentation**: 4 spaces
- **Naming conventions**:
  - Classes: `PascalCase` (e.g., `DBConnector`, `ProducerStateTable`)
  - Methods: `camelCase`
  - Variables: `camelCase` or `m_` prefix for members
  - Constants: `UPPER_CASE`
  - Namespaces: `swss`
- **SWIG bindings**: Python bindings auto-generated via SWIG in `pyext/`

## Build Instructions

```bash
# Install dependencies
sudo apt-get install make libtool m4 autoconf dh-exec debhelper cmake pkg-config \
  libhiredis-dev libnl-3-dev libnl-genl-3-dev libnl-route-3-dev \
  libnl-nf-3-dev swig3.0 libpython3-dev libgtest-dev libgmock-dev libboost-dev

# Build Google Test
cd /usr/src/gtest && sudo cmake . && sudo make

# Build from source
./autogen.sh
./configure
make && sudo make install

# Build Debian package
./autogen.sh
./configure
dpkg-buildpackage -us -uc -b

# Build with debug/test support
./autogen.sh
./configure --enable-debug 'CXXFLAGS=-O0 -g'
make clean && make
```

## Testing

```bash
# Start Redis with keyspace notifications
sudo sed -i 's/notify-keyspace-events ""/notify-keyspace-events AKE/' /etc/redis/redis.conf
sudo service redis-server start

# Run unit tests
tests/tests
```

- Tests use **Google Test / Google Mock** framework
- Tests are in `tests/` directory
- Redis server must be running with keyspace notifications enabled
- Tests validate database operations, producer/consumer patterns, and netlink handling

## PR Guidelines

- **Commit format**: `[component]: Description`
- **Signed-off-by**: REQUIRED (`git commit -s`)
- **CLA**: Sign Linux Foundation EasyCLA
- **Testing**: Add tests for new public APIs
- **ABI compatibility**: Be careful about breaking ABI — this library is linked by many components
- **SWIG bindings**: If adding new C++ APIs, update SWIG interface files in `pyext/` if Python access is needed

## Common Patterns

### Database Connection
```cpp
#include "dbconnector.h"
// Connect to a specific database
swss::DBConnector db("APPL_DB", 0);
// Or by database ID
swss::DBConnector db(APPL_DB, "localhost", 6379, 0);
```

### Producer/Consumer Tables
```cpp
// Producer (writer)
swss::ProducerStateTable producer(&db, "MY_TABLE");
std::vector<swss::FieldValueTuple> fvs;
fvs.emplace_back("field1", "value1");
producer.set("key1", fvs);

// Consumer (reader)
swss::ConsumerStateTable consumer(&db, "MY_TABLE");
swss::Select s;
s.addSelectable(&consumer);
swss::Selectable *sel;
s.select(&sel);  // Blocks until data available
```

### Select Loop Pattern
```cpp
swss::Select s;
s.addSelectable(&consumer1);
s.addSelectable(&consumer2);
while (true) {
    swss::Selectable *sel;
    int ret = s.select(&sel);
    if (ret == swss::Select::OBJECT) {
        // Process the selectable that has data
    }
}
```

## Dependencies

- **Redis / hiredis**: In-memory database and C client library
- **libnl**: Netlink library suite (libnl-3, libnl-route-3, libnl-genl-3)
- **Boost**: Serialization and utility libraries
- **SWIG**: For generating Python bindings
- **libzmq**: ZeroMQ for additional messaging patterns

## Gotchas

- **ABI stability**: This is a shared library — changes to class layouts or virtual functions break ABI for all consumers
- **Redis dependency**: All tests require a running Redis server
- **Keyspace notifications**: Redis must have `notify-keyspace-events AKE` enabled
- **Thread safety**: Most classes are NOT thread-safe; use one instance per thread
- **Python bindings lag**: After C++ changes, SWIG bindings may need regeneration
- **Schema changes**: Adding new table names to `schema.h` affects all downstream repos
- **Lua scripts**: Some operations use Redis Lua scripts in `common/`; these must be deployed to `/usr/share/swss/`
- **Namespace support**: Multi-ASIC support requires namespace-aware database connections
