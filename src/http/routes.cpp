#include "http/routes.h"

using namespace Pistache;

namespace HttpHelpers {

std::optional<json> parseBody(const Rest::Request& req) {
    try {
        if (req.body().empty()) return json::object();
        auto j = json::parse(req.body(), /*cb*/nullptr, /*allow_exceptions*/false);
        if (j.is_discarded()) return std::nullopt;
        return j;
    } catch (...) {
        return std::nullopt;
    }
}

void sendJson(Http::ResponseWriter res, Http::Code code, const json& body) {
    res.headers().add<Http::Header::ContentType>(MIME(Application, Json));
    res.send(code, body.dump());
}

std::string qp(const Rest::Request& req, const std::string& key, const std::string& def) {
    auto v = req.query().get(key);
    if (v) return v.get();
    return def;
}

void cors(Http::ResponseWriter& res) {
    res.headers().add<Http::Header::AccessControlAllowOrigin>("*");
    res.headers().add<Http::Header::AccessControlAllowMethods>("GET, POST, OPTIONS");
    res.headers().add<Http::Header::AccessControlAllowHeaders>("Content-Type, Authorization");
}

} // namespace HttpHelpers
