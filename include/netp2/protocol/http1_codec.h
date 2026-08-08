#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "netp2/protocol/request_context.h"

namespace netp2::protocol {

/// HTTP/1.1 解析配置
struct Http1CodecConfig {
    /// Header 总大小上限 (bytes)
    std::size_t max_header_size = 8192;

    /// Header 数量上限
    std::size_t max_headers = 100;

    /// 是否强制 HTTP/1.1 请求必须包含 Host header
    bool require_host_header = true;
};

/// HTTP/1.1 Codec（基于 llhttp）
/// 不暴露 llhttp 类型，对业务层透明
class Http1Codec {
public:
    explicit Http1Codec(const Http1CodecConfig& config = {});
    ~Http1Codec();

    // 禁止拷贝
    Http1Codec(const Http1Codec&) = delete;
    Http1Codec& operator=(const Http1Codec&) = delete;

    // 支持移动
    Http1Codec(Http1Codec&&) noexcept;
    Http1Codec& operator=(Http1Codec&&) noexcept;

    /// 解析 HTTP/1.1 请求
    /// @param buffer 请求数据缓冲区
    /// @param ctx 输出参数，解析成功时填充
    /// @return 解析错误码
    ParseError parse(std::string_view buffer, RequestContext& ctx);

    /// 重置解析器状态（用于连接复用场景）
    void reset();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace netp2::protocol
