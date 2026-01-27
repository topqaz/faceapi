#pragma once
#include <string>
#include <vector>
#include <json/json.h> // JsonCpp

namespace api {
    namespace model {

        // 单个人脸结果
        struct FaceItem {
            std::string name;
            float confidence;
            bool is_known;
            int x;
            int y;
            int width;
            int height;

            Json::Value toJson() const {
                Json::Value v;
                v["name"] = name;
                v["confidence"] = confidence;
                v["is_known"] = is_known;
                v["x"] = x;
                v["y"] = y;
                v["width"] = width;
                v["height"] = height;
                return v;
            }
        };

        // 整体响应结构
        struct RecognitionResponse {
            std::string status;
            int count;
            std::vector<FaceItem> faces;

            Json::Value toJson() const {
                Json::Value v;
                v["status"] = status;
                v["count"] = count;

                Json::Value facesArray(Json::arrayValue);
                for (const auto& face : faces) {
                    facesArray.append(face.toJson());
                }
                v["faces"] = facesArray;
                return v;
            }
        };

    }
}