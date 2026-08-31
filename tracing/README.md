# Server Tracing (bpftrace)

`trace_server.bt` uses eBPF/bpftrace to observe `server1`'s runtime behavior without modifying or recompiling the server itself.

## What it measures
- Total connections accepted (`sys_enter_accept` tracepoint)
- Threads created per connection (`pthread_create` uprobe)
- Request handling latency, end-to-end (`handle_client` entry/return)
- Mutex wait time on the shared cache lock (`pthread_mutex_lock` uprobe)
- Cache misses on the `/` path specifically, via `fopen` calls as a proxy (`fopen` only runs on the `/` miss branch — misses on `/about` or unknown paths use hardcoded strings and don't call `fopen`, so this undercounts misses if the test hits multiple paths)

## How to run it

1. Compile with debug symbols (no code changes, just adds symbol names): gcc -g -o server1 server1.c -lpthread
2. Start the server: ./server1
3. Run the trace script (separate terminal): sudo bpftrace tracing/trace_server.bt
4. Generate load (separate terminal): ab -n 500 -c 50 http://localhost:8080/


## Findings (from a clean 500-request / 50-concurrent run, single path `/`)

- 1 `fopen` call across ~500 requests to `/` — consistent with a near-total cache hit rate for repeated requests to the same path (this run only tested one path, so it doesn't exercise the cache's miss/eviction behavior — see Known Limitations)
- Most requests completed in under 2µs (cache hits are fast)
- Mutex wait time had a long tail: roughly 1 in 5 requests waited over 1ms for the cache lock, up to 16ms, under 50 concurrent threads — all serialized on one exclusive mutex even though most operations are reads, 
- 500 threads created for 500 connections — confirms real per-connection thread creation overhead (thread-per-connection model, no pooling)

## Known limitations

- The cache-miss measurement (`fopen` count) only reflects misses on the `/` path. A general miss counter would need a probe placed directly at the `found_in_cache == 0` branch in the code, which isn't reachable via a function-entry uprobe without adding an instrumentation point to the source.
- The 2-slot cache (`CACHE_SIZE 2`) has no eviction — once both slots fill, any further distinct paths are permanent misses. This script wasn't run against a multi-path load test yet, so that behavior is inferred from the code, not directly measured here.

## Why this matters

The mutex wait tail is direct evidence that a plain mutex doesn't scale well here, since reads and writes both fully serialize on it. 
Planned next steps: swap to an rwlock (allow concurrent reads, exclusive only on writes) and move to a fixed-size thread pool instead of creating a new thread per connection.
