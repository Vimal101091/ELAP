#!/bin/bash
set -euo pipefail

STORAGE_DEMO="$1"
TEST_DB="/tmp/elap_storage_integration_test.db"

cleanup() {
    rm -f "$TEST_DB"
}
trap cleanup EXIT

echo "=== Storage Round-Trip Integration Test ==="

export ELAP_STORAGE_DEMO_DB="$TEST_DB"

output=$("$STORAGE_DEMO" 2>&1)

echo "$output"

echo "$output" | grep -q "greeting = hello world"
echo "$output" | grep -q "count = 42"
echo "$output" | grep -q "enabled = true"
echo "$output" | grep -q "binary blob size ="
echo "$output" | grep -q "Keys (4):"
echo "$output" | grep -q "has count after remove = false"
echo "$output" | grep -q "device.name after reopen = controller-1"
echo "$output" | grep -q "startup.delay after reopen = 5"
echo "$output" | grep -q "logging.enabled after reopen = true"
echo "$output" | grep -q "Storage demo complete."

echo "=== All storage integration checks passed ==="
