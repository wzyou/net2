#!/usr/bin/env python3
"""
D-BENCHMARK-BASELINE: wrk 结果解析脚本
参考：16-phase-d-implementation-guide.md 第 16.14.3 节
"""

import sys
import json
import re
from typing import Dict, Any


def convert_to_ms(value: str, unit: str) -> float:
    """将延迟值转换为毫秒"""
    val = float(value)
    if unit == 'us':
        return val / 1000.0
    elif unit == 'ms':
        return val
    elif unit == 's':
        return val * 1000.0
    return val


def parse_wrk_output(text: str) -> Dict[str, Any]:
    """解析 wrk 输出文本"""
    result = {}
    
    # Parse Requests/sec
    match = re.search(r'Requests/sec:\s+([\d.]+)', text)
    if match:
        result['rps'] = float(match.group(1))
    
    # Parse Latency distribution
    # wrk 输出格式示例：
    #   50%    1.23ms
    #   75%    2.34ms
    #   90%    3.45ms
    #   99%    4.56ms
    
    p50_match = re.search(r'50%\s+([\d.]+)(us|ms|s)', text)
    p75_match = re.search(r'75%\s+([\d.]+)(us|ms|s)', text)
    p90_match = re.search(r'90%\s+([\d.]+)(us|ms|s)', text)
    p99_match = re.search(r'99%\s+([\d.]+)(us|ms|s)', text)
    
    if p50_match:
        result['p50_ms'] = convert_to_ms(p50_match.group(1), p50_match.group(2))
    if p75_match:
        result['p75_ms'] = convert_to_ms(p75_match.group(1), p75_match.group(2))
    if p90_match:
        result['p90_ms'] = convert_to_ms(p90_match.group(1), p90_match.group(2))
    if p99_match:
        result['p99_ms'] = convert_to_ms(p99_match.group(1), p99_match.group(2))
    
    # Parse error rate (Non-2xx or 3xx responses)
    error_match = re.search(r'Non-2xx or 3xx responses:\s+(\d+)', text)
    total_match = re.search(r'(\d+) requests in', text)
    if error_match and total_match:
        errors = int(error_match.group(1))
        total = int(total_match.group(1))
        result['error_rate'] = (errors / total * 100) if total > 0 else 0
        result['total_requests'] = total
        result['error_count'] = errors
    elif total_match:
        total = int(total_match.group(1))
        result['error_rate'] = 0.0
        result['total_requests'] = total
        result['error_count'] = 0
    
    # Parse transfer rate
    transfer_match = re.search(r'Transfer/sec:\s+([\d.]+)(KB|MB|GB)', text)
    if transfer_match:
        value = float(transfer_match.group(1))
        unit = transfer_match.group(2)
        # 转换为 MB/s
        if unit == 'KB':
            value = value / 1024.0
        elif unit == 'GB':
            value = value * 1024.0
        result['transfer_mbps'] = value
    
    return result


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <wrk_output.txt> <output.json>", file=sys.stderr)
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    try:
        with open(input_file, 'r') as f:
            wrk_output = f.read()
    except IOError as e:
        print(f"Error reading input file: {e}", file=sys.stderr)
        sys.exit(1)
    
    result = parse_wrk_output(wrk_output)
    
    if not result:
        print("Warning: No data parsed from wrk output", file=sys.stderr)
    
    try:
        with open(output_file, 'w') as f:
            json.dump(result, f, indent=2)
    except IOError as e:
        print(f"Error writing output file: {e}", file=sys.stderr)
        sys.exit(1)
    
    # 输出到 stdout 供人查看
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
