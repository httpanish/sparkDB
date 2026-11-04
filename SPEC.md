# sparkDB Spec (draft)

## 1. Purpose

sparkDB is a single-node, in-memory key-value database written from scratch in
C++20, in the style of Redis. Its main goal is learning C++ and database
internals, so a simple design is preferred over a fast or feature-rich one.

## 2. Scope

**In scope**

- Strings, lists, hashes, and sets as value types
- Key expiry (TTL)
- A local REPL and a TCP server that handles many clients
- A subset of the RESP2 protocol, so `redis-cli` works as a test client
- Optional durability through an append-only log and snapshots
- A memory limit with eviction

**Out of scope**

- SQL, or a relational data model
- Replication and clustering
- Authentication and TLS
- Multi-command transactions
- Platforms other than Linux (the server uses `epoll`)

**Dependencies**

No third-party libraries in the database itself. Tests use Catch2.

## 3. Architecture

```
  REPL (stdin)        TCP server (epoll)
       |                     |
  line tokenizer        RESP parser/encoder
       \                    /
        Command dispatcher          <- no I/O; args in, Response out
                |
             KvStore                <- data and expiry; knows nothing about commands
                |
           Persistence              <- append-only log, snapshots
```

Design rules:

1. **Commands are independent of I/O.** A command takes a list of arguments and
   returns a `Response` value, so the REPL, the server, and log replay can all
   share one code path.
2. **Commands run on one thread.** They run one at a time and each is atomic, so
   the store needs no locks. This is the Redis model.
3. **Time is injectable.** Expiry reads time through a clock the tests can
   control, so tests never need to sleep.

## 4. Data model

- Keys and string values are binary-safe: they may contain any byte, including
  `\0`.
- Each key holds one value of one type.
- Using a command on a key of the wrong type returns a `WRONGTYPE` error and
  leaves the data unchanged.
- A collection (list, hash, set) that becomes empty is deleted, along with its key.
- An expired key behaves exactly like a key that doesn't exist.

## 5. Commands

Command names are case-insensitive. A wrong argument count or an unknown command
returns an error and never stops the server.

| Group      | Commands                                                |
|------------|---------------------------------------------------------|
| Connection | `PING`, `ECHO`, `QUIT`                                  |
| Keys       | `DEL`, `EXISTS`, `TYPE`, `DBSIZE`, `FLUSHALL`           |
| Strings    | `GET`, `SET [EX seconds]`, `APPEND`, `INCR`, `DECR`     |
| Expiry     | `EXPIRE`, `TTL`, `PERSIST`                              |
| Lists      | `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LLEN`, `LRANGE`      |
| Hashes     | `HSET`, `HGET`, `HDEL`, `HLEN`, `HGETALL`               |
| Sets       | `SADD`, `SREM`, `SISMEMBER`, `SCARD`, `SMEMBERS`        |
| Admin      | `INFO`, `SAVE`                                          |

Return values and error messages should match Redis where practical, so its
documentation can serve as the reference.

## 6. Network protocol

- Listens on TCP, port 6379 by default.
- Requests are RESP2 arrays of bulk strings. Inline commands aren't supported.
- Replies use simple strings, errors, integers, bulk strings, and arrays,
  including null replies.
- Pipelining is supported: a client may send several requests without waiting,
  and replies come back in the same order.
- A malformed request or an oversized one (over 64 MiB) gets an error reply, and
  the connection is closed.
- `redis-cli` sends `COMMAND DOCS` when it starts. Replying with an
  unknown-command error is acceptable.

## 7. Durability

Set by the `appendfsync` config option:

| Mode       | What a crash can lose                  |
|------------|----------------------------------------|
| `off`      | Everything (pure in-memory, default)   |
| `no`       | Whatever the OS hasn't flushed yet     |
| `everysec` | About the last second of writes        |
| `always`   | Nothing that was acknowledged          |

- The log stores each write command in RESP format, which reuses the parser.
- Startup loads the latest snapshot, if any, and then replays the log.
- A truncated final log entry, the normal result of a crash, is dropped with a
  warning. Corruption anywhere else stops startup with an error.
- A snapshot must be written to a temp file and renamed into place, so a crash
  can't leave a half-written snapshot behind.
- Expiry is stored as an absolute time, so a restart doesn't extend a key's life.

## 8. Quality bar

- Compiles without warnings under `-Wall -Wextra -Wpedantic`.
- Every feature ships with tests.
- Tests pass under AddressSanitizer and UBSan.
- A malformed client request can never crash the server.

## 9. Features

Each feature should fit in one sitting and one small commit, and should finish
with the program working. Do them in order; a phase depends on the ones before it.

### Phase 0: Foundation

| ID   | Feature                 | Done when                                                 | You'll learn          |
|------|-------------------------|-----------------------------------------------------------|-----------------------|
| 0.1  | Project skeleton        | CMake builds a core library and an executable, no warnings | CMake targets        |
| 0.2  | Catch2 + CTest          | `ctest` runs one test against the core library            | Test setup, CTest     |
| 0.3  | KvStore                 | Get, Set, and Delete are tested, including missing keys and values containing `\0` | Writing unit tests, API design |
| 0.4  | REPL loop               | It echoes input, and exits on `exit` and Ctrl-D           | Stream state, EOF     |

### Phase 1: Commands

| ID   | Feature             | Done when                                                 | You'll learn               |
|------|---------------------|-----------------------------------------------------------|----------------------------|
| 1.1  | Tokenizer           | `SET a b` becomes `["SET", "a", "b"]`                     | `std::string_view`, parsing |
| 1.2  | Quoted tokens       | `SET a "b c\n"` works; an unclosed quote is an error      | Small state machines       |
| 1.3  | `Response` type     | It holds every reply kind and can print itself            | `std::variant`             |
| 1.4  | Dispatcher + `PING` | Case-insensitive lookup; unknown commands return an error | Function tables, lambdas   |
| 1.5  | Argument checks     | Each command declares its arity, checked in one place     | Table-driven design        |
| 1.6  | `GET`, `SET`, `DEL` | The REPL can store and read data                          | Layering                   |
| 1.7  | `EXISTS`, `DBSIZE`, `FLUSHALL`, `ECHO` | All work; `DEL` and `EXISTS` take several keys | Extending the table |
| 1.8  | `APPEND`, `INCR`, `DECR` | Non-integer values and overflow return errors        | `std::from_chars`, overflow |

### Phase 2: Expiry

| ID   | Feature             | Done when                                                  | You'll learn            |
|------|---------------------|------------------------------------------------------------|-------------------------|
| 2.1  | Injectable clock    | Tests can set the time by hand                             | `std::chrono`, seams    |
| 2.2  | `EXPIRE`, `TTL`     | `TTL` returns -2 for no key, -1 for no expiry              | Storing per-key metadata |
| 2.3  | Lazy expiry         | Reading an expired key deletes it and reports it missing   | Lazy vs. eager work     |
| 2.4  | `PERSIST`, `SET EX` | Both work, and a plain `SET` clears an existing expiry     | Command options         |

### Phase 3: Networking

| ID   | Feature               | Done when                                                  | You'll learn               |
|------|-----------------------|------------------------------------------------------------|----------------------------|
| 3.1  | RESP encoder          | Each `Response` kind encodes to the right bytes            | Wire formats               |
| 3.2  | RESP parser           | Parses a complete request from a buffer                    | Length-prefixed framing    |
| 3.3  | Partial input         | Input split at any byte still parses; bad input is an error | Incremental parsing        |
| 3.4  | Single-client server  | One `redis-cli` can connect and run commands               | Sockets, RAII for file descriptors |
| 3.5  | `epoll` event loop    | Two clients connected at once both work                    | Non-blocking I/O, `epoll`  |
| 3.6  | Per-client buffers    | Slow readers and large replies work; handles partial writes | Backpressure               |
| 3.7  | Pipelining            | 1000 pipelined `PING`s return 1000 ordered replies         | Buffer management          |
| 3.8  | Active expiry         | Expired keys that are never read still get removed         | Timers, sampling           |
| 3.9  | Clean shutdown        | SIGINT/SIGTERM stops the server cleanly; no ASan errors    | Signals, `signalfd`        |

### Phase 4: Data types

| ID   | Feature             | Done when                                                  | You'll learn               |
|------|---------------------|------------------------------------------------------------|----------------------------|
| 4.1  | Typed values        | Values live in a `std::variant`; `TYPE` works; `WRONGTYPE` errors work | `std::variant`, `std::visit` |
| 4.2  | Lists: push, pop, len | `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, and `LLEN` work; an empty list's key is deleted | Container choice |
| 4.3  | `LRANGE`            | Negative indexes and out-of-range bounds work              | Edge-case handling         |
| 4.4  | Hashes              | All hash commands work                                     | Nested containers          |
| 4.5  | Sets                | All set commands work                                      | `std::unordered_set`       |

### Phase 5: Persistence

| ID   | Feature             | Done when                                                  | You'll learn               |
|------|---------------------|------------------------------------------------------------|----------------------------|
| 5.1  | Write flag          | Each command is marked as a write or a read                | Command metadata           |
| 5.2  | Log writes          | Successful writes are appended to the log file             | File I/O                   |
| 5.3  | Log replay          | Data survives a restart                                    | Recovery by replay         |
| 5.4  | Absolute expiry     | The log records `EXPIRE` as an absolute time; TTLs stay correct after restart | Why replay must be deterministic |
| 5.5  | fsync modes         | `no`, `everysec`, and `always` all work                    | `fsync`, durability costs  |
| 5.6  | Crash recovery      | A truncated tail is recovered; corruption elsewhere stops startup | Failure modes        |
| 5.7  | Snapshot save       | `SAVE` writes a binary file atomically                     | Serialization, atomic rename |
| 5.8  | Snapshot load       | Startup loads the snapshot, then replays the log           | Checksums, versioned formats |

### Phase 6: Memory limits

| ID   | Feature             | Done when                                                  | You'll learn               |
|------|---------------------|------------------------------------------------------------|----------------------------|
| 6.1  | `INFO` stats        | Reports key count, commands run, and connected clients     | Observability              |
| 6.2  | Memory accounting   | Tracks an estimate of bytes used, tested against known data | Allocation overhead       |
| 6.3  | `maxmemory`         | Writes over the limit return an error                      | Config, failure policy     |
| 6.4  | Approximate LRU     | Evicts by sampling keys instead of rejecting the write     | Sampling vs. exact LRU     |

### Phase 7: Stretch goals (pick any)

| ID   | Feature             | Why it's interesting                                       |
|------|---------------------|------------------------------------------------------------|
| 7.1  | Your own hash table | Replaces `std::unordered_map`; teaches probing and resizing |
| 7.2  | Incremental rehash  | Growing the table without long pauses                      |
| 7.3  | Sorted sets         | Build a skip list (`ZADD`, `ZRANGE`)                       |
| 7.4  | Log compaction      | Shrinks the log by rewriting it from current state         |
| 7.5  | Benchmark           | Measure with `redis-benchmark`, then profile with `perf`   |

## 10. Decisions

1. **Data model:** Redis-style key-value.
2. **Protocol:** A subset of RESP2, so `redis-cli` works as a test client.
3. **Concurrency:** A single-threaded `epoll` event loop for now. This can be
   revisited later.
4. **Test framework:** Catch2.
