#!/usr/bin/env bash
set -u

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <ipc_event_receiver> <ipc_event_publisher>" >&2
    exit 2
fi

receiver_binary="$1"
publisher_binary="$2"
bus_name="/elap_event_process_$$"
workdir="$(mktemp -d)"
receiver_log="$workdir/receiver.log"
publisher_log="$workdir/publisher.log"
receiver_pid=""

cleanup()
{
    if [ -n "$receiver_pid" ] && kill -0 "$receiver_pid" 2>/dev/null; then
        kill -TERM "$receiver_pid" 2>/dev/null || true
        wait "$receiver_pid" 2>/dev/null || true
    fi
    rm -rf "$workdir"
}
trap cleanup EXIT

"$receiver_binary" --bus "$bus_name" > "$receiver_log" 2>&1 &
receiver_pid="$!"

attempts=50
while [ "$attempts" -gt 0 ]; do
    if grep -q "^ready$" "$receiver_log"; then
        break
    fi
    if ! kill -0 "$receiver_pid" 2>/dev/null; then
        echo "receiver exited before ready" >&2
        cat "$receiver_log" >&2
        exit 1
    fi
    attempts=$((attempts - 1))
    sleep 0.1
done

if ! grep -q "^ready$" "$receiver_log"; then
    echo "timed out waiting for receiver" >&2
    cat "$receiver_log" >&2
    exit 1
fi

"$publisher_binary" --bus "$bus_name" --topic "phase2.event" --payload "running" > "$publisher_log" 2>&1
publisher_status="$?"
if [ "$publisher_status" -ne 0 ]; then
    echo "publisher exited with status $publisher_status" >&2
    cat "$publisher_log" >&2
    cat "$receiver_log" >&2
    exit 1
fi

wait "$receiver_pid"
receiver_status="$?"
receiver_pid=""

if [ "$receiver_status" -ne 0 ]; then
    echo "receiver exited with status $receiver_status" >&2
    cat "$receiver_log" >&2
    exit 1
fi

grep -q "^phase2.event=running$" "$receiver_log" || {
    echo "receiver did not print expected event" >&2
    cat "$receiver_log" >&2
    exit 1
}

exit 0
