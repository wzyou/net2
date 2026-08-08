#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace netp2::protocol {

/// HTTP 请求上下文，协议解析后的统一抽象
/// 遮蔽 HTTP/1.1、HTTP/2 差异，业务层只消费此结构
struct RequestContext {
    /// HTTP method (GET, POST, etc.)
    std::string method;

    /// Request target / path (e.g., /api/v1/resource)
    std::string target;

    /// HTTP version (e.g., "HTTP/1.1", "HTTP/2")
    std::string version;

    /// Host header value (HTTP/1.1 MUST have Host)
    std::string host;

    /// All headers (key -> value)
    /// 注意：header name 在解析时已转小写
    std::unordered_map<std::string, std::string> headers;

    /// Request body view (指向缓冲区，不持有所有权)
    std::string_view body;

    /// Client endpoint (IP:port for logging/routing)
    std::string client_endpoint;

    /// Trace context (W3C traceparent, 预留)
    std::string trace_parent;

    /// Content-Length (如果存在)
    std::size_t content_length = 0;

    /// 是否显式设置了 Content-Length
    bool has_content_length = false;
};

/// HTTP 解析错误码
enum class ParseError {
    kSuccess = 0,              ///< 解析成功
    kNeedMoreData,             ///< 需要更多数据才能完成解析
    kInvalidRequestLine,       ///< 请求行格式错误
    kMissingHost,              ///< HTTP/1.1 缺失 Host header
    kHeaderTooLarge,           ///< Header 总大小超限
    kTooManyHeaders,           ///< Header 数量超限
    kContentLengthMismatch,    ///< Content-Length 与实际不一致
    kInternalError,            ///< 内部错误
};

/// 将错误码转换为 HTTP 状态码
inline int parse_error_to_http_status(ParseError error) {
    switch (error) {
    case ParseError::kSuccess:
        return 200;
    case ParseError::kNeedMoreData:
        return 400;  // 不完整请求视为客户端错误
    case ParseError::kInvalidRequestLine:
        return 400;
    case ParseError::kMissingHost:
        return 400;
    case ParseError::kHeaderTooLarge:
        return 431;  // Request Header Fields Too Large
    case ParseError::kTooManyHeaders:
        return 431;
    case ParseError::kContentLengthMismatch:
        return 400;
    case ParseError::kInternalError:
        return 500;
    }
    return 500;
}

/// 将错误码转换为描述字符串
inline std::string_view parse_error_to_string(ParseError error) {
    switch (error) {
    case ParseError::kSuccess:
        return "success";
    case ParseError::kNeedMoreData:
        return "need more data";
    case ParseError::kInvalidRequestLine:
        return "invalid request line";
    case ParseError::kMissingHost:
        return "missing host header";
    case ParseError::kHeaderTooLarge:
        return "header too large";
    case ParseError::kTooManyHeaders:
        return "too many headers";
    case ParseError::kContentLengthMismatch:
        return "content-length mismatch";
    case ParseError::kInternalError:
        return "internal error";
    }
    return "unknown error";
}

}  // namespace netp2::protocol
