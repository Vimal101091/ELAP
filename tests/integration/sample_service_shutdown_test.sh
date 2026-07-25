#!/usr/bin/env bash
set -u

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <sample_service_binary>" >&2
    exit 2
fi

sample_service="$1"
if [ ! -x "$sample_service" ]; then
    echo "sample service is not executable: $sample_service" >&2
    exit 2
fi

workdir="$(mktemp -d)"
config_file="$workdir/sample_service.conf"
log_file="$workdir/sample_service.log"
service_pid=""

cleanup()
{
    if [ -n "$service_pid" ] && kill -0 "$service_pid" 2>/dev/null; then
        kill -TERM "$service_pid" 2>/dev/null || true
        wait "$service_pid" 2>/dev/null || true
    fi
    rm -rf "$workdir"
}
trap cleanup EXIT

cat > "$config_file" <<EOF
service.name=sample_service
log.level=info
worker.count=2
heartbeat.interval_ms=100
shutdown.timeout_ms=5000
EOF

"$sample_service" --config "$config_file" > "$log_file" 2>&1 &
service_pid="$!"

wait_for_log()
{
    pattern="$1"
    attempts=50
    while [ "$attempts" -gt 0 ]; do
        if grep -q "$pattern" "$log_file"; then
            return 0
        fi
        if ! kill -0 "$service_pid" 2>/dev/null; then
            echo "sample service exited before log appeared: $pattern" >&2
            cat "$log_file" >&2
            return 1
        fi
        attempts=$((attempts - 1))
        sleep 0.1
    done

    echo "timed out waiting for log: $pattern" >&2
    cat "$log_file" >&2
    return 1
}

wait_for_log "service running" || exit 1
wait_for_log "heartbeat from heartbeat_0" || exit 1
wait_for_log "heartbeat from heartbeat_1" || exit 1

kill -TERM "$service_pid"
wait "$service_pid"
exit_status="$?"
service_pid=""

if [ "$exit_status" -ne 0 ]; then
    echo "sample service exited with status $exit_status" >&2
    cat "$log_file" >&2
    exit 1
fi

grep -q "stopping service" "$log_file" || {
    echo "missing shutdown start log" >&2
    cat "$log_file" >&2
    exit 1
}

grep -q "thread stopped: heartbeat_0" "$log_file" || {
    echo "missing heartbeat_0 stop log" >&2
    cat "$log_file" >&2
    exit 1
}

grep -q "thread stopped: heartbeat_1" "$log_file" || {
    echo "missing heartbeat_1 stop log" >&2
    cat "$log_file" >&2
    exit 1
}

grep -q "service stopped" "$log_file" || {
    echo "missing service stopped log" >&2
    cat "$log_file" >&2
    exit 1
}

exit 0
