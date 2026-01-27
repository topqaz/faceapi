//
// Created by root on 2026/1/20.
//

#ifndef FACEPROJECT_AUTH_H
#define FACEPROJECT_AUTH_H

#include <crow.h>
#include <string>
#include <jwt-cpp/jwt.h>
#include <unordered_map>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>
static const std::string JWT_SECRET = "my_super_secret_key";
static const std::string JWT_ISSUER = "FaceAPI";

std::string createToken(const std::string& username);
bool authFromRequest(const crow::request& req, std::string& username);


crow::response authFailResponse();


#endif //FACEPROJECT_AUTH_H