#include "request_handler.h"

namespace http_handler {
    StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                    bool keep_alive,
                                    boost::string_view content_type = ContentType::TEXT_HTML) {
        StringResponse response(status, http_version);
        if(status == http::status::method_not_allowed) response.set(http::field::allow, beast::string_view("GET, HEAD"));
        response.set(http::field::content_type, content_type);
        response.body() = body;
        response.content_length(body.size());
        response.keep_alive(keep_alive);
        return response;
    }
}  // namespace http_handler
