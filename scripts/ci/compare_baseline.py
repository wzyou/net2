#!/usr/bin/env python3
"""
D-BENCHMARK-CI: 基线对比脚本
参考：16-phase-d-implementation-guide.md 第 16.4.2 节

对比当前基准运行结果与历史基线，判断是否发生性能退化。
"""

import sys
import json
from typing import Dict, Any, Optional


# 性能回归阈值（参考 benchmarks/baseline_report.md）
THRESHOLDS = {
    'rps_drop_percent': 5.0,        # RPS 下降不得超过 5%
    'p99_increase_percent': 10.0,   # P99 延迟上升不得超过 10%
    'error_rate_increase': 0.2,     # 错误率上升不得超过 0.2%
}


def load_json(filepath: str) -> Optional[Dict[str, Any]]:
    """加载 JSON 文件"""
    try:
        with open(filepath, 'r') as f:
            return json.load(f)
    except Exception as e:
        print(f"Error loading {filepath}: {e}", file=sys.stderr)
        return None


def compare_metrics(baseline: Dict[str, Any], current: Dict[str, Any]) -> bool:
    """
    对比指标，返回是否通过门禁
    
    Returns:
        True: 通过门禁
        False: 性能退化，失败
    """
    passed = True
    
    print("Baseline vs Current:")
    print("-" * 60)
    
    # 检查 RPS
    baseline_rps = baseline.get('rps', 0)
    current_rps = current.get('rps', 0)
    if baseline_rps > 0:
        rps_change_percent = ((current_rps - baseline_rps) / baseline_rps) * 100
        rps_status = "✓" if rps_change_percent >= -THRESHOLDS['rps_drop_percent'] else "✗"
        print(f"{rps_status} RPS: {baseline_rps:.2f} → {current_rps:.2f} ({rps_change_percent:+.2f}%)")
        
        if rps_change_percent < -THRESHOLDS['rps_drop_percent']:
            print(f"  ⚠️  RPS dropped by {-rps_change_percent:.2f}%, exceeds threshold ({THRESHOLDS['rps_drop_percent']}%)")
            passed = False
    else:
        print("⚠️  Baseline RPS not available")
    
    # 检查 P99 延迟
    baseline_p99 = baseline.get('p99_ms', 0)
    current_p99 = current.get('p99_ms', 0)
    if baseline_p99 > 0:
        p99_change_percent = ((current_p99 - baseline_p99) / baseline_p99) * 100
        p99_status = "✓" if p99_change_percent <= THRESHOLDS['p99_increase_percent'] else "✗"
        print(f"{p99_status} P99: {baseline_p99:.2f}ms → {current_p99:.2f}ms ({p99_change_percent:+.2f}%)")
        
        if p99_change_percent > THRESHOLDS['p99_increase_percent']:
            print(f"  ⚠️  P99 increased by {p99_change_percent:.2f}%, exceeds threshold ({THRESHOLDS['p99_increase_percent']}%)")
            passed = False
    else:
        print("⚠️  Baseline P99 not available")
    
    # 检查错误率
    baseline_error_rate = baseline.get('error_rate', 0)
    current_error_rate = current.get('error_rate', 0)
    error_rate_change = current_error_rate - baseline_error_rate
    error_rate_status = "✓" if error_rate_change <= THRESHOLDS['error_rate_increase'] else "✗"
    print(f"{error_rate_status} Error Rate: {baseline_error_rate:.2f}% → {current_error_rate:.2f}% ({error_rate_change:+.2f}%)")
    
    if error_rate_change > THRESHOLDS['error_rate_increase']:
        print(f"  ⚠️  Error rate increased by {error_rate_change:.2f}%, exceeds threshold ({THRESHOLDS['error_rate_increase']}%)")
        passed = False
    
    print("-" * 60)
    
    # 额外指标展示（不影响门禁）
    print("\nAdditional metrics (informational):")
    print(f"  P50: {baseline.get('p50_ms', 0):.2f}ms → {current.get('p50_ms', 0):.2f}ms")
    print(f"  P90: {baseline.get('p90_ms', 0):.2f}ms → {current.get('p90_ms', 0):.2f}ms")
    print(f"  Total Requests: {baseline.get('total_requests', 0)} → {current.get('total_requests', 0)}")
    
    return passed


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <baseline.json> <current.json>", file=sys.stderr)
        sys.exit(1)
    
    baseline_file = sys.argv[1]
    current_file = sys.argv[2]
    
    baseline = load_json(baseline_file)
    current = load_json(current_file)
    
    if baseline is None or current is None:
        print("Failed to load baseline or current metrics", file=sys.stderr)
        sys.exit(1)
    
    print(f"Baseline: {baseline_file}")
    print(f"Current:  {current_file}")
    print("")
    
    if compare_metrics(baseline, current):
        print("\n✓ Performance benchmark passed")
        sys.exit(0)
    else:
        print("\n✗ Performance regression detected")
        sys.exit(1)


if __name__ == '__main__':
    main()
