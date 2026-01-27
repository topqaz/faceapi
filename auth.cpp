#include "auth.h"
#include <iostream>

std::string createToken(const std::string& username) {
    using namespace std::chrono;

    auto token = jwt::create()
        .set_issuer(JWT_ISSUER)
        .set_type("JWT")
        .set_issued_at(system_clock::now())
        .set_expires_at(system_clock::now() + hours(24))
        .set_payload_claim("username", jwt::claim(username))
        .sign(jwt::algorithm::hs256{JWT_SECRET});

    return token;
}

bool verifyToken(const std::string& token, std::string& username_out) {
    try {
        // 1. 解析 token（不做校验）
        auto decoded = jwt::decode(token);

        // 2. 构建校验器
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{JWT_SECRET})
            .with_issuer(JWT_ISSUER);

        // 3. 执行校验（签名 + exp + iss）
        verifier.verify(decoded);

        // 4. 读取 payload
        username_out = decoded.get_payload_claim("username").as_string();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[JWT] verify failed: " << e.what() << std::endl;
        return false;
    }
}


bool authFromRequest(const crow::request& req, std::string& username) {
    auto auth = req.get_header_value("Authorization");
    if (auth.empty()) return false;

    // 必须是 Bearer token
    if (auth.rfind("Bearer ", 0) != 0) return false;

    std::string token = auth.substr(7);
    return verifyToken(token, username);
}

crow::response authFailResponse() {
    crow::json::wvalue res;
    res["code"] = 401;
    res["msg"] = "Unauthorized";
    return crow::response(401, res);
}