#!/usr/bin/env bash
set -u

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <sample_ipc_server> <sample_ipc_client>" >&2
    exit 2
fi

server_binary="$1"
client_binary="$2"

if [ ! -x "$server_binary" ]; then
    echo "server is not executable: $server_binary" >&2
    exit 2
fi

if [ ! -x "$client_binary" ]; then
    echo "client is not executable: $client_binary" >&2
    exit 2
fi

workdir="$(mktemp -d)"
socket_path="$workdir/sample_ipc_server.sock"
config_file="$workdir/sample_ipc_server.conf"
server_log="$workdir/server.log"
client_log="$workdir/client.log"
server_pid=""

cleanup()
{
    if [ -n "$server_pid" ] && kill -0 "$server_pid" 2>/dev/null; then
        kill -TERM "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
    rm -rf "$workdir"
}
trap cleanup EXIT

cat > "$config_file" <<EOF
service.name=sample_ipc_server
log.level=info
ipc.socket_path=$socket_path
EOF

"$server_binary" --config "$config_file" > "$server_log" 2>&1 &
server_pid="$!"

wait_for_log()
{
    pattern="$1"
    attempts=50
    while [ "$attempts" -gt 0 ]; do
        if grep -q "$pattern" "$server_log"; then
            return 0
        fi
        if ! kill -0 "$server_pid" 2>/dev/null; then
            echo "server exited before log appeared: $pattern" >&2
            cat "$server_log" >&2
            return 1
        fi
        attempts=$((attempts - 1))
        sleep 0.1
    done

    echo "timed out waiting for log: $pattern" >&2
    cat "$server_log" >&2
    return 1
}

wait_for_log "service running" || exit 1
wait_for_log "listening on $socket_path" || exit 1

"$client_binary" --socket "$socket_path" --message "status" > "$client_log" 2>&1
client_status="$?"

if [ "$client_status" -ne 0 ]; then
    echo "client exited with status $client_status" >&2
    cat "$client_log" >&2
    cat "$server_log" >&2
    exit 1
fi

response="$(tr -d '\r' < "$client_log")"
if [ "$response" != "sample_ipc_server:status" ]; then
    echo "unexpected client response: $response" >&2
    cat "$client_log" >&2
    cat "$server_log" >&2
    exit 1
fi

wait_for_log "received request: status" || exit 1

kill -TERM "$server_pid"
wait "$server_pid"
server_status="$?"
server_pid=""

if [ "$server_status" -ne 0 ]; then
    echo "server exited with status $server_status" >&2
    cat "$server_log" >&2
    exit 1
fi

grep -q "requesting IPC shutdown" "$server_log" || {
    echo "missing IPC shutdown log" >&2
    cat "$server_log" >&2
    exit 1
}

grep -q "thread stopped: ipc_listener" "$server_log" || {
    echo "missing listener thread stop log" >&2
    cat "$server_log" >&2
    exit 1
}

grep -q "service stopped" "$server_log" || {
    echo "missing service stopped log" >&2
    cat "$server_log" >&2
    exit 1
}

exit 0
