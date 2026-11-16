# Multiplayer load and regression testing

The Linux load harness exercises an embedded `ServerHost` through real transports
and independently authored temporary game data. It needs no optional user data.
SQLite uses Unix sockets and one shared persisted character; PostgreSQL uses TLS
and separate owned accounts/characters. Both create independent runtime entities.
The PostgreSQL fixture creates and drops its own unique schema in the configured
test database. Temporary certificates, maps and catalogs are generated locally.

## Build and correctness checks

Use the README dependencies, including the PostgreSQL client development package
when enabling that backend. A separate Release build keeps coverage and sanitizer
instrumentation out of profiling:

```sh
cmake -S . -B build-load -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DOPENKAL_ENGINE=null \
  -DOPENKAL_BUILD_TESTS=ON -DOPENKAL_BUILD_TOOLS=OFF \
  -DOPENKAL_BUILD_AREA_EDITOR=OFF -DOPENKAL_BUILD_PROFILE_CLIENT=OFF \
  -DOPENKAL_WITH_POSTGRES=ON
cmake --build build-load --target \
  openkal_test_host_runtime_metrics openkal_multiplayer_load_test -j4
ctest --test-dir build-load \
  -R '^(host_runtime_metrics|multiplayer_load_sqlite)$' --output-on-failure
```

Set `OPENKAL_TEST_POSTGRES_DSN` to a test database DSN, then run:

```sh
ctest --test-dir build-load -R '^multiplayer_load_postgres$' --output-on-failure
```

Without a DSN the PostgreSQL wrapper returns skip code 77. It generates its own
short-lived TLS certificate and chooses a local port. The correctness profiles
use four sessions, one warmup round and three measured rounds. Queue bounds,
state, conservation, ordering and coarse watchdogs determine success; measured
latencies have no acceptance threshold.

The workload checks gameplay handshakes, every healthy peer's movement and
authoritative talent responses, one ordinary chat per connection, actor combat,
guaranteed drops/pickup, stale and truncated talent requests, a portal/ACK, and
durable exit/reconnect. PostgreSQL additionally races two peers for one dropped
item and checks global IID uniqueness and quantity conservation. Pickup first
checks rejection from 96 pixels away without item/money changes, then approaches
each drop's wire position using acknowledged ordinary movement before collecting
its exact world identity. A slow reader
is included in the requested population when there is more than one client.
Its read-only talent requests produce ordinary authoritative replies. Drain-only
intervals separate healthy-peer replies from the aggregate pending-output witness;
bounded adaptive bursts resume when that output drains. The test pauses slow-peer
submissions to observe sustained queued output, healthy-peer progress and world
ticks before the existing output deadline closes that peer. It retains the
production queue limits and security gates.

SQLite background sessions finish saving before the actor's final save because
they share the same persisted character. Exact final pose, money, talents and
complete inventory IID/count pairs are checked through persistence and reconnect.
This smoke profile does not establish independent SQLite multiplayer identities.

## Optional population profiles

Run these serially in an otherwise idle measurement window, using the same build,
configuration and round counts for comparisons:

```sh
python3 tests/test_multiplayer_load.py build-load/tests/openkal_multiplayer_load_test \
  --clients 1 --warmup 2 --rounds 10 --report /tmp/openkal-load-1.json
python3 tests/test_multiplayer_load.py build-load/tests/openkal_multiplayer_load_test \
  --clients 16 --warmup 2 --rounds 10 --report /tmp/openkal-load-16.json
python3 tests/test_multiplayer_load.py build-load/tests/openkal_multiplayer_load_test \
  --clients 64 --warmup 2 --rounds 10 --report /tmp/openkal-load-64.json
```

One client has no slow reader. The 64-client profile releases provisioning/control
connections before filling all gameplay slots. Client count includes the slow
reader; report fields identify the remaining healthy population during pressure.

## Measurement meaning

`barrier_ms` measures completion of an entire all-peer movement/talent FIFO round,
with one sample per measured round. Warmup removes only those barrier samples;
the initial round also observes ordinary chat. These are not individual-client
packet latencies. A small sample count makes p95/p99 observed order statistics,
often the maximum, rather than a reliable tail-latency estimate.

Host timings cover completed main-loop iterations across the scenario, including
connection setup/provisioning, warmup, mixed gameplay, pressure and session cleanup
performed inside those loops. Worker startup and final shutdown persistence/drain
outside the loop are excluded. Dispatch work includes work before and
after poll, including scheduled ticks, and excludes the blocking poll wait and
metrics publication. Scheduled tick duration measures the actual existing block,
including world-lock wait, local destruction and unlock. Scheduling lateness is
derived from the existing elapsed time and tick interval. These are elapsed work
durations, not CPU accounting or a steady-state throughput benchmark.

Independent 4,096-entry dispatch/tick rings retain their latest chronological
samples. `completed_loops` and `scheduled_ticks` are lifetime totals; retained
sample counts disclose the measured windows. Queue peaks are observations at
completed-loop boundaries and are lower bounds on true enqueue peaks. They can
include active and retiring connections. Peak RSS belongs to the combined harness,
Host and clients process; it excludes the separate PostgreSQL server process.
Profiling itself adds overhead, and results describe these synthetic fixtures.

Runtime collection is off by default. Configure it with
`ServerHost::set_runtime_metrics_enabled` before starting the worker; live or
unjoined-worker reconfiguration is refused. `runtime_metrics_snapshot` remains
available after shutdown. Re-enabling while stopped resets the recorder. Recording
allocates no sample memory; snapshots copy under a separate mutex and allocate
their result vectors outside the recorder lock, without acquiring the world lock.
