#!/usr/bin/env bash
# D-BENCHMARK-BASELINE: wrk 基准执行脚本
# 参考：16-phase-d-implementation-guide.md 第 16.14.1 节

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOST="localhost"
PORT="18080"
DURATION="30s"
THREADS="4"
CONNECTIONS="100"
OUTPUT=""

while [[ $# -gt 0 ]]; do
  case $1 in
    --host) HOST="$2"; shift 2 ;;
    --port) PORT="$2"; shift 2 ;;
    --duration) DURATION="$2"; shift 2 ;;
    --threads) THREADS="$2"; shift 2 ;;
    --connections) CONNECTIONS="$2"; shift 2 ;;
    --output) OUTPUT="$2"; shift 2 ;;
    -h|--help)
      echo "Usage: $0 [options]"
      echo "Options:"
      echo "  --host HOST         Target host (default: localhost)"
      echo "  --port PORT         Target port (default: 18080)"
      echo "  --duration DURATION Test duration (default: 30s)"
      echo "  --threads THREADS   Thread count (default: 4)"
      echo "  --connections CONN  Connection count (default: 100)"
      echo "  --output FILE       Output JSON file"
      echo "  -h, --help          Show this help message"
      exit 0
      ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

if [[ -z "$OUTPUT" ]]; then
  OUTPUT="$SCRIPT_DIR/baseline_$(date +%Y%m%d_%H%M%S).json"
fi

# 检查 wrk 是否安装
if ! command -v wrk &> /dev/null; then
  echo "Error: wrk not found. Please install wrk first."
  echo "macOS: brew install wrk"
  echo "Ubuntu: sudo apt-get install wrk"
  exit 1
fi

echo "[$(date +%T)] Running wrk baseline..."
echo "  Target: http://$HOST:$PORT"
echo "  Duration: $DURATION, Threads: $THREADS, Connections: $CONNECTIONS"

wrk -t "$THREADS" -c "$CONNECTIONS" -d "$DURATION" \
  --latency \
  -s "$SCRIPT_DIR/baseline.lua" \
  "http://$HOST:$PORT/proxy/mock" \
  > "$SCRIPT_DIR/wrk_output.txt"

echo "[$(date +%T)] Parsing results..."
python3 "$SCRIPT_DIR/parse_results.py" \
  "$SCRIPT_DIR/wrk_output.txt" \
  "$OUTPUT"

echo "[$(date +%T)] Results saved to: $OUTPUT"
cat "$OUTPUT"
