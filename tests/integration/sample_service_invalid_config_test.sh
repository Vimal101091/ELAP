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
config_file="$workdir/invalid_sample_service.conf"
log_file="$workdir/sample_service.log"

cleanup()
{
    rm -rf "$workdir"
}
trap cleanup EXIT

cat > "$config_file" <<EOF
service.name=sample_service
invalid line without separator
EOF

"$sample_service" --config "$config_file" > "$log_file" 2>&1
exit_status="$?"

if [ "$exit_status" -eq 0 ]; then
    echo "sample service unexpectedly accepted invalid configuration" >&2
    cat "$log_file" >&2
    exit 1
fi

grep -q "invalid configuration line" "$log_file" || {
    echo "missing invalid configuration error log" >&2
    cat "$log_file" >&2
    exit 1
}

grep -q "starting service" "$log_file" && {
    echo "sample service started after invalid configuration" >&2
    cat "$log_file" >&2
    exit 1
}

exit 0
