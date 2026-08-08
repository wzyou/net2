#!/usr/bin/env bash
set -euo pipefail

url="${1:-http://127.0.0.1:8080/healthz}"
curl --fail --silent --show-error "$url" > /dev/null
echo "ok"
