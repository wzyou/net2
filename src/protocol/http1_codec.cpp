#include "netp2/protocol/http1_codec.h"

#include <algorithm>
#include <cctype>
#include <cstring>

// 使用 llhttp 作为底层 HTTP/1.1 解析引擎
// 如果系统未安装 llhttp，需要在 third_party/ 引入或通过包管理器安装
// macOS: brew install llhttp
// Ubuntu: apt-get install libllhttp-dev
#include <llhttp.h>

namespace netp2::protocol {

namespace {

// 将 header name 转为小写（HTTP header name 不区分大小写）
std::string to_lower(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return result;
}

}  // namespace

// Http1Codec 的 Pimpl 实现，隐藏 llhttp 细节
class Http1Codec::Impl {
public:
    explicit Impl(const Http1CodecConfig& config) : config_(config) {
        llhttp_settings_init(&settings_);

        // 设置回调
        settings_.on_message_begin = on_message_begin;
        settings_.on_url = on_url;
        settings_.on_header_field = on_header_field;
        settings_.on_header_value = on_header_value;
        settings_.on_headers_complete = on_headers_complete;
        settings_.on_body = on_body;
        settings_.on_message_complete = on_message_complete;

        llhttp_init(&parser_, HTTP_REQUEST, &settings_);
        parser_.data = this;
    }

    ParseError parse(std::string_view buffer, RequestContext& ctx) {
        // 重置输出上下文
        ctx_ = &ctx;
        ctx_->method.clear();
        ctx_->target.clear();
        ctx_->version.clear();
        ctx_->host.clear();
        ctx_->headers.clear();
        ctx_->body = {};
        ctx_->content_length = 0;
        ctx_->has_content_length = false;

        current_header_field_.clear();
        current_header_value_.clear();
        total_header_size_ = 0;
        parse_error_ = ParseError::kSuccess;

        // 执行解析
        const llhttp_errno_t err = llhttp_execute(
            &parser_,
            buffer.data(),
            buffer.size());

        // 检查解析器错误
        if (err != HPE_OK && err != HPE_PAUSED_UPGRADE) {
            // llhttp 报错，转换为我们的错误码
            if (err == HPE_INVALID_METHOD || err == HPE_INVALID_URL || 
                err == HPE_INVALID_VERSION) {
                return ParseError::kInvalidRequestLine;
            }
            return ParseError::kInternalError;
        }

        // 检查自定义错误（在回调中设置）
        if (parse_error_ != ParseError::kSuccess) {
            return parse_error_;
        }

        // 填充版本信息
        ctx_->version = "HTTP/" + std::to_string(parser_.http_major) + "." +
                        std::to_string(parser_.http_minor);

        // 填充 method
        ctx_->method = llhttp_method_name(static_cast<llhttp_method_t>(parser_.method));

        // HTTP/1.1 必须有 Host header
        if (config_.require_host_header && 
            parser_.http_major == 1 && parser_.http_minor >= 1 &&
            ctx_->host.empty()) {
            return ParseError::kMissingHost;
        }

        // 验证 Content-Length 一致性
        if (ctx_->has_content_length && ctx_->body.size() != ctx_->content_length) {
            return ParseError::kContentLengthMismatch;
        }

        return ParseError::kSuccess;
    }

    void reset() {
        llhttp_reset(&parser_);
        current_header_field_.clear();
        current_header_value_.clear();
        total_header_size_ = 0;
        parse_error_ = ParseError::kSuccess;
        ctx_ = nullptr;
    }

private:
    // llhttp 回调函数
    static int on_message_begin([[maybe_unused]] llhttp_t* parser) {
        return 0;
    }

    static int on_url(llhttp_t* parser, const char* at, size_t length) {
        auto* self = static_cast<Impl*>(parser->data);
        if (self->ctx_) {
            self->ctx_->target.append(at, length);
        }
        return 0;
    }

    static int on_header_field(llhttp_t* parser, const char* at, size_t length) {
        auto* self = static_cast<Impl*>(parser->data);
        
        // 如果之前有 value，说明上一个 header 完成，保存它
        if (!self->current_header_value_.empty()) {
            self->save_current_header();
        }

        self->current_header_field_.append(at, length);
        self->total_header_size_ += length;

        // 检查 header 总大小
        if (self->total_header_size_ > self->config_.max_header_size) {
            self->parse_error_ = ParseError::kHeaderTooLarge;
            return -1;
        }

        return 0;
    }

    static int on_header_value(llhttp_t* parser, const char* at, size_t length) {
        auto* self = static_cast<Impl*>(parser->data);
        self->current_header_value_.append(at, length);
        self->total_header_size_ += length;

        // 检查 header 总大小
        if (self->total_header_size_ > self->config_.max_header_size) {
            self->parse_error_ = ParseError::kHeaderTooLarge;
            return -1;
        }

        return 0;
    }

    static int on_headers_complete(llhttp_t* parser) {
        auto* self = static_cast<Impl*>(parser->data);
        
        // 保存最后一个 header
        if (!self->current_header_field_.empty()) {
            self->save_current_header();
        }

        // 检查 header 数量
        if (self->ctx_ && self->ctx_->headers.size() > self->config_.max_headers) {
            self->parse_error_ = ParseError::kTooManyHeaders;
            return -1;
        }

        return 0;
    }

    static int on_body(llhttp_t* parser, const char* at, size_t length) {
        auto* self = static_cast<Impl*>(parser->data);
        if (self->ctx_) {
            // body 使用 string_view 指向原始缓冲区
            self->ctx_->body = std::string_view(at, length);
        }
        return 0;
    }

    static int on_message_complete([[maybe_unused]] llhttp_t* parser) {
        return 0;
    }

    void save_current_header() {
        if (!ctx_) {
            return;
        }

        const std::string field_lower = to_lower(current_header_field_);
        
        // 特殊处理 Host header
        if (field_lower == "host") {
            ctx_->host = current_header_value_;
        }

        // 特殊处理 Content-Length
        if (field_lower == "content-length") {
            try {
                ctx_->content_length = std::stoull(current_header_value_);
                ctx_->has_content_length = true;
            } catch (...) {
                // Content-Length 解析失败，保持 has_content_length = false
            }
        }

        // 保存到 headers map
        ctx_->headers[field_lower] = current_header_value_;

        current_header_field_.clear();
        current_header_value_.clear();
    }

    Http1CodecConfig config_;
    llhttp_t parser_;
    llhttp_settings_t settings_;

    RequestContext* ctx_ = nullptr;
    std::string current_header_field_;
    std::string current_header_value_;
    std::size_t total_header_size_ = 0;
    ParseError parse_error_ = ParseError::kSuccess;
};

// Http1Codec 公开接口实现
Http1Codec::Http1Codec(const Http1CodecConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

Http1Codec::~Http1Codec() = default;

Http1Codec::Http1Codec(Http1Codec&&) noexcept = default;
Http1Codec& Http1Codec::operator=(Http1Codec&&) noexcept = default;

ParseError Http1Codec::parse(std::string_view buffer, RequestContext& ctx) {
    return impl_->parse(buffer, ctx);
}

void Http1Codec::reset() {
    impl_->reset();
}

}  // namespace netp2::protocol
