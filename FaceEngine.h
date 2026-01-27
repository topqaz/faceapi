#pragma once

// Windows 宏定义处理 (放在最前面)
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <windows.h>
#endif

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <opencv2/opencv.hpp>
#include <seeta/FaceDetector.h>
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceRecognizer.h>
#include "db.h"
// 识别结果结构体
struct FaceResult {
    cv::Rect box;           // 人脸坐标
    std::string name;       // 识别出的名字 (未识别则为 "Unknown")
    float confidence;       // 相似度/置信度
    bool isKnown;           // 是否是已知人员
};

class FaceEngine {
private:
    seeta::FaceDetector* detector = nullptr;
    seeta::FaceLandmarker* landmarker = nullptr;
    seeta::FaceRecognizer* recognizer = nullptr;

    // 内存中的特征数据库
    std::map<std::string, std::vector<float>> face_database;

    // 内部辅助：提取单张图片的特征
    bool extractFeaturesInternal(const SeetaImageData& image, std::vector<float>& features);

public:
    FaceEngine();
    ~FaceEngine();
    FaceDB faceDB;
    bool initDatabase(const std::string& db_path);
    bool addFace(const cv::Mat& image,
                 const std::string& person_id,
                 const std::string& phone,
                 const std::string& gender);

    // 1. 初始化模型
    bool init(const std::string& det_model, const std::string& land_model, const std::string& rec_model);

    // 2. 数据库操作
    // bool loadDatabase(const std::string& filename);
    // bool saveDatabase(const std::string& filename);
    // bool addFace(const cv::Mat& image, const std::string& person_id); // 注册人脸
    // bool deleteFace(const std::string& person_id);
    // std::map<std::string, std::vector<float>> getDatabase() const;

    // 3. 核心识别功能
    // 检测并识别一帧图像中的所有人脸
    std::vector<FaceResult> detectAndRecognize(const cv::Mat& frame, float threshold = 0.6f);
    
    // 单纯比较两张图片
    float compareTwoImages(const std::string& imgPath1, const std::string& imgPath2);

    // 新增：单例获取方法
    static std::shared_ptr<FaceEngine> getInstance() {
        static std::shared_ptr<FaceEngine> instance = std::make_shared<FaceEngine>();
        return instance;
    }

    bool isInitialized = false;
};