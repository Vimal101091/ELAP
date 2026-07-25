#!/usr/bin/env bash
set -u

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <ipc_echo_server> <ipc_echo_client>" >&2
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
socket_path="$workdir/ipc_echo.sock"
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

"$server_binary" "$socket_path" > "$server_log" 2>&1 &
server_pid="$!"

attempts=50
while [ "$attempts" -gt 0 ]; do
    if [ -S "$socket_path" ]; then
        break
    fi
    if ! kill -0 "$server_pid" 2>/dev/null; then
        echo "server exited before creating socket" >&2
        cat "$server_log" >&2
        exit 1
    fi
    attempts=$((attempts - 1))
    sleep 0.1
done

if [ ! -S "$socket_path" ]; then
    echo "timed out waiting for server socket" >&2
    cat "$server_log" >&2
    exit 1
fi

"$client_binary" "$socket_path" "phase2-ipc" > "$client_log" 2>&1
client_status="$?"

if [ "$client_status" -ne 0 ]; then
    echo "client exited with status $client_status" >&2
    cat "$client_log" >&2
    cat "$server_log" >&2
    exit 1
fi

response="$(tr -d '\r' < "$client_log")"
if [ "$response" != "echo:phase2-ipc" ]; then
    echo "unexpected client response: $response" >&2
    cat "$client_log" >&2
    cat "$server_log" >&2
    exit 1
fi

wait "$server_pid"
server_status="$?"
server_pid=""

if [ "$server_status" -ne 0 ]; then
    echo "server exited with status $server_status" >&2
    cat "$server_log" >&2
    exit 1
fi

exit 0
