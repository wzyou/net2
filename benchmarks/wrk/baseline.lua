-- D-BENCHMARK-BASELINE: wrk 请求模板
-- 参考：16-phase-d-implementation-guide.md 第 16.14.2 节

request = function()
  headers = {
    ["Host"] = "example.com",
    ["User-Agent"] = "wrk-benchmark/1.0",
    ["Accept"] = "*/*"
  }
  return wrk.format("GET", "/proxy/mock", headers, nil)
end

-- 可选：记录延迟分布
-- done = function(summary, latency, requests)
--   io.write("Latency distribution:\n")
--   for _, p in pairs({ 50, 75, 90, 99, 99.9 }) do
--     n = latency:percentile(p)
--     io.write(string.format("  %g%%: %d us\n", p, n))
--   end
-- end
