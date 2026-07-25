#!/usr/bin/env bash
set -u

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <ipc_mq_receiver> <ipc_mq_sender>" >&2
    exit 2
fi

receiver_binary="$1"
sender_binary="$2"
queue_name="/elap_mq_process_$$"
workdir="$(mktemp -d)"
receiver_log="$workdir/receiver.log"
sender_log="$workdir/sender.log"
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

"$receiver_binary" --queue "$queue_name" > "$receiver_log" 2>&1 &
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

"$sender_binary" --queue "$queue_name" --message "mq-process-payload" > "$sender_log" 2>&1
sender_status="$?"
if [ "$sender_status" -ne 0 ]; then
    echo "sender exited with status $sender_status" >&2
    cat "$sender_log" >&2
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

grep -q "^mq-process-payload$" "$receiver_log" || {
    echo "receiver did not print expected payload" >&2
    cat "$receiver_log" >&2
    exit 1
}

exit 0
