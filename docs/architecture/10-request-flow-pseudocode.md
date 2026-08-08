# 10. 单请求代理主流程（伪代码）

以下伪代码由 v1.0 全量设计整理为可实现版本，强调 RCU 安全、限流、熔断与连接复用顺序。

```text
Coroutine ProcessSingleRequest(client_socket, req_ctx):
    # 1) 读取当前核心最新路由快照（RCU 安全持有）
    route_table = atomic_load_acquire(g_thread_local_route_table)

    # 2) 路由匹配: Host + Path + ClientIP
    route = route_table.match(req_ctx.host, req_ctx.path, req_ctx.client_ip)
    if route is null:
        co_await SendHttpResponse(client_socket, 404, "Route Not Found")
        metrics.record("unknown", 404)
        return

    # 3) thread_local 限流
    if not route.limiter.try_acquire_or_delay():
        co_await SendHttpResponse(client_socket, 429, "Too Many Requests")
        metrics.record(route.path, 429)
        return

    # 4) 选择健康 upstream（跳过 breaker=open 节点）
    node = route.select_healthy_node()
    if node is null:
        co_await SendHttpResponse(client_socket, 503, "All Upstreams Tripped")
        metrics.record(route.path, 503)
        return

    # 5) breaker 前置校验
    if not node.breaker.allow_request():
        co_await SendHttpResponse(client_socket, 503, "Service Circuit Open")
        metrics.record(route.path, 503)
        return

    # 6) 获取 upstream 连接（优先 thread_local 池复用）
    upstream_socket = local_pool.acquire(node.target)
    if upstream_socket is null:
        try:
            upstream_socket = co_await (AsyncConnect(node.target) || AsyncSleep(5s))
        except ConnectTimeout:
            node.breaker.on_result(false)
            co_await SendHttpResponse(client_socket, 504, "Upstream Connect Timeout")
            return

    # 7) 转发与响应代理（含读写超时）
    start = now_ns()
    ok = false
    try:
        co_await AsyncWriteHttp1Request(upstream_socket, req_ctx)
        co_await ProxyResponseToClient(upstream_socket, client_socket)
        ok = true
    except:
        ok = false

    # 8) 更新熔断与指标
    cost_ns = now_ns() - start
    node.breaker.on_result(ok)
    metrics.record(route.path, req_ctx.response_status, cost_ns)

    # 9) 连接归还或销毁
    if ok and req_ctx.keep_alive and upstream_socket.is_open():
        local_pool.release(node.target, upstream_socket)
    else:
        upstream_socket.close()
```

## 关键不变式
- 同一请求在单核心内完成主要处理闭环。
- 任何配置快照在协程作用域内必须强引用持有。
- 熔断状态与请求结果更新应在同一线程上下文完成。
