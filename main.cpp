#include "FaceEngine.h"




#include "auth.h"

#include <crow.h>
#include <crow/multipart.h>

#include <jwt-cpp/jwt.h>
#include <crow/middlewares/cors.h>
#include <unordered_map>
#include <iostream>
#include <cstdlib>
#include <chrono>








static std::unordered_map<std::string, std::string> USER_TABLE = {
    {"admin", "123456"},
    {"user",  "123456"}
};


int main(int argc, char** argv) {
    // ---------------- FaceEngine 初始化 ----------------
    FaceEngine engine;
    engine.initDatabase("face.db");

    if (!engine.init("models/face_detector.csta",
                     "models/face_landmarker_pts5.csta",
                     "models/face_recognizer_light.csta")) {
        std::cerr << "FaceEngine init failed!" << std::endl;
        return 1;
    }

    crow::App<crow::CORSHandler> app;




    auto& cors = app.get_middleware<crow::CORSHandler>();

    cors.global()

        .headers("Content-Type", "Authorization", "X-Custom-Header")


        .methods("POST"_method, "GET"_method, "OPTIONS"_method)


        .origin("*");

    // ---------------- 登录接口 ----------------
    CROW_ROUTE(app, "/api/auth/login").methods("POST"_method)
    ([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        crow::json::wvalue res;

        if (!body) {
            res["code"] = -1;
            res["message"] = "invalid json";
            return crow::response(400, res);
        }

        std::string username = body["username"].s();
        std::string password = body["password"].s();

        if (!USER_TABLE.count(username) ||
            USER_TABLE[username] != password) {
            res["code"] = -2;
            res["message"] = "invalid username or password";
            return crow::response(401, res);
        }

        res["code"] = 0;
        res["data"]["token"] = createToken(username);
        res["data"]["user"]["username"] = username;
        return crow::response(res);
    });

    // ---------------- 获取人脸列表 ----------------
    CROW_ROUTE(app, "/api/face/list").methods("GET"_method)
    ([&engine](const crow::request& req) {
        crow::json::wvalue res;
        std::vector<FaceRecord> records;

        std::string username;
        if (!authFromRequest(req, username)) {
            return authFailResponse();
        }


        if (!engine.faceDB.getAllFaces(records)) {
            res["code"] = -1;
            res["msg"] = "db error";
            return crow::response(500, res);
        }

        res["code"] = 0;
        crow::json::wvalue list = crow::json::wvalue::list();

        int i = 0;
        for (auto& r : records) {
            list[i]["name"] = r.person_id;
            list[i]["phone"] = r.phone;
            list[i]["gender"] = r.gender;
            ++i;
        }

        res["faces"] = std::move(list);
        return crow::response(res);
    });

    // ---------------- 添加人脸 ----------------
    CROW_ROUTE(app, "/api/face/add").methods("POST"_method)
    ([&engine](const crow::request& req) {
        crow::multipart::message msg(req);
        crow::json::wvalue res;

        if (msg.parts.size() < 4) {
            res["code"] = -1;
            res["msg"] = "invalid form";
            return crow::response(400, res);
        }

        auto& img_part = msg.parts[0];
        std::string name   = msg.parts[1].body;
        std::string phone  = msg.parts[2].body;
        std::string gender = msg.parts[3].body;

        std::vector<uchar> data(img_part.body.begin(), img_part.body.end());
        cv::Mat img = cv::imdecode(data, cv::IMREAD_COLOR);

        if (img.empty()) {
            res["code"] = -2;
            res["msg"] = "invalid image";
            return crow::response(400, res);
        }

        if (!engine.addFace(img, name, phone, gender)) {
            res["code"] = -3;
            res["msg"] = "add face failed";
            return crow::response(500, res);
        }

        res["code"] = 0;
        res["msg"] = "success";
        return crow::response(res);
    });

    // ---------------- 删除人脸（JWT） ----------------
    CROW_ROUTE(app, "/api/face/delete").methods("POST"_method)
    ([&engine](const crow::request& req) {
        std::string username;
        if (!authFromRequest(req, username)) {
            return authFailResponse();
        }

        auto body = crow::json::load(req.body);
        crow::json::wvalue res;

        if (!body || !body.has("name")) {
            res["code"] = -1;
            res["msg"] = "missing name";
            return crow::response(400, res);
        }

        if (!engine.faceDB.deleteFace(body["name"].s())) {
            res["code"] = -2;
            res["msg"] = "not found";
            return crow::response(404, res);
        }

        res["code"] = 0;
        res["msg"] = "deleted";
        return crow::response(res);
    });

    // ---------------- 人脸识别 ----------------
    CROW_ROUTE(app, "/api/face").methods("POST"_method)
    ([&engine](const crow::request& req) {
        crow::multipart::message msg(req);
        crow::json::wvalue res;

        if (msg.parts.empty()) {
            res["code"] = -1;
            res["msg"] = "no file";
            return crow::response(400, res);
        }

        std::vector<uchar> data(msg.parts[0].body.begin(),
                           msg.parts[0].body.end());
        cv::Mat img = cv::imdecode(data, cv::IMREAD_COLOR);
        // cv::resize(img,img,cv::Size(640,480));
        if (img.empty()) {
            res["code"] = -2;
            res["msg"] = "invalid image";
            return crow::response(400, res);
        }
        auto start = std::chrono::steady_clock::now();
        auto results = engine.detectAndRecognize(img, 0.6f);
        auto end = std::chrono::steady_clock::now();
        auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::cout << "[FaceAPI] cost time: " << cost << " ms" << std::endl;
        std::vector<FaceRecord> db;
        engine.faceDB.getAllFaces(db);

        res["code"] = 0;
        crow::json::wvalue arr = crow::json::wvalue::list();

        int i = 0;
        for (auto& f : results) {
            arr[i]["x"] = f.box.x;
            arr[i]["y"] = f.box.y;
            arr[i]["w"] = f.box.width;
            arr[i]["h"] = f.box.height;
            arr[i]["name"] = f.name;
            arr[i]["confidence"] = f.confidence;

            if (f.isKnown) {
                auto it = find_if(db.begin(), db.end(),
                    [&](auto& r){ return r.person_id == f.name; });
                if (it != db.end()) {
                    arr[i]["phone"] = it->phone;
                    arr[i]["gender"] = it->gender;
                }
            }
            ++i;
        }

        res["faces"] = std::move(arr);

        return crow::response(res);
    });

    std::cout << "Face API running on http://0.0.0.0:18080" << std::endl;
    app.port(18080).multithreaded().run();

}
