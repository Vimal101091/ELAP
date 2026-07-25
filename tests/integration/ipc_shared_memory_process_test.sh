#!/usr/bin/env bash
set -u

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <ipc_shm_writer> <ipc_shm_reader>" >&2
    exit 2
fi

writer_binary="$1"
reader_binary="$2"
region_name="/elap_shm_process_$$"
payload="shared-memory-process-payload"
workdir="$(mktemp -d)"
writer_log="$workdir/writer.log"
reader_log="$workdir/reader.log"
writer_pid=""

cleanup()
{
    if [ -n "$writer_pid" ] && kill -0 "$writer_pid" 2>/dev/null; then
        kill -TERM "$writer_pid" 2>/dev/null || true
        wait "$writer_pid" 2>/dev/null || true
    fi
    rm -rf "$workdir"
}
trap cleanup EXIT

"$writer_binary" --name "$region_name" --message "$payload" --hold-ms 1000 > "$writer_log" 2>&1 &
writer_pid="$!"

attempts=50
while [ "$attempts" -gt 0 ]; do
    if grep -q "^ready$" "$writer_log"; then
        break
    fi
    if ! kill -0 "$writer_pid" 2>/dev/null; then
        echo "writer exited before ready" >&2
        cat "$writer_log" >&2
        exit 1
    fi
    attempts=$((attempts - 1))
    sleep 0.1
done

if ! grep -q "^ready$" "$writer_log"; then
    echo "timed out waiting for writer" >&2
    cat "$writer_log" >&2
    exit 1
fi

"$reader_binary" --name "$region_name" --bytes "${#payload}" > "$reader_log" 2>&1
reader_status="$?"
if [ "$reader_status" -ne 0 ]; then
    echo "reader exited with status $reader_status" >&2
    cat "$reader_log" >&2
    cat "$writer_log" >&2
    exit 1
fi

grep -q "^$payload$" "$reader_log" || {
    echo "reader did not print expected payload" >&2
    cat "$reader_log" >&2
    cat "$writer_log" >&2
    exit 1
}

wait "$writer_pid"
writer_status="$?"
writer_pid=""

if [ "$writer_status" -ne 0 ]; then
    echo "writer exited with status $writer_status" >&2
    cat "$writer_log" >&2
    exit 1
fi

exit 0
