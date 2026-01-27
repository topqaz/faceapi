#include "FaceEngine.h"
#include <fstream>
#include <iostream>

FaceEngine::FaceEngine() {}

FaceEngine::~FaceEngine() {
    if (detector) delete detector;
    if (landmarker) delete landmarker;
    if (recognizer) delete recognizer;
}

bool FaceEngine::init(const std::string& det_model, const std::string& land_model, const std::string& rec_model) {
    try {
        seeta::ModelSetting detector_setting;
        detector_setting.set_device(SEETA_DEVICE_CPU);
        detector_setting.append(det_model);
        detector = new seeta::FaceDetector(detector_setting);

        seeta::ModelSetting landmarker_setting;
        landmarker_setting.set_device(SEETA_DEVICE_CPU);
        landmarker_setting.append(land_model);
        landmarker = new seeta::FaceLandmarker(landmarker_setting);

        seeta::ModelSetting recognizer_setting;
        recognizer_setting.set_device(SEETA_DEVICE_CPU);
        recognizer_setting.append(rec_model);
        recognizer = new seeta::FaceRecognizer(recognizer_setting);

        std::cout << "[FaceEngine] Models initialized successfully." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[FaceEngine] Init failed: " << e.what() << std::endl;
        return false;
    }
}

// 内部函数：从SeetaImage提取特征
bool FaceEngine::extractFeaturesInternal(const SeetaImageData& image, std::vector<float>& features) {
    auto faces = detector->detect(image);
    if (faces.size == 0) return false;

    // 默认只处理最大的人脸(第0个)进行特征提取
    std::vector<SeetaPointF> points(landmarker->number());
    landmarker->mark(image, faces.data[0].pos, points.data());

    features.resize(recognizer->GetExtractFeatureSize());
    recognizer->Extract(image, points.data(), features.data());
    return true;
}


bool FaceEngine::initDatabase(const std::string& db_path) {
    return faceDB.open(db_path);
}

bool FaceEngine::addFace(const cv::Mat& image,
                         const std::string& person_id,
                         const std::string& phone,
                         const std::string& gender) {
    SeetaImageData img{ image.cols, image.rows, image.channels(), image.data };

    std::vector<float> features;
    if (!extractFeaturesInternal(img, features)) return false;

    FaceRecord record;
    record.person_id = person_id;
    record.phone = phone;
    record.gender = gender;
    record.feature = std::move(features);

    return faceDB.addFace(record);
}

// 核心：检测并识别当前帧
std::vector<FaceResult> FaceEngine::detectAndRecognize(const cv::Mat& frame, float threshold) {
    std::vector<FaceResult> results;
    if (frame.empty()) return results;

    // 转 SeetaImageData
    SeetaImageData seeta_image;
    seeta_image.width = frame.cols;
    seeta_image.height = frame.rows;
    seeta_image.channels = frame.channels();
    seeta_image.data = frame.data;

    // 1. 检测所有人脸
    auto faces = detector->detect(seeta_image);

    // 2. 获取数据库中所有记录
    std::vector<FaceRecord> db_records;
    faceDB.getAllFaces(db_records); // 从 SQLite 读取 person_id + feature + phone + gender

    // 3. 遍历检测到的人脸进行识别
    for (int i = 0; i < faces.size; ++i) {
        FaceResult res;
        res.box = cv::Rect(faces.data[i].pos.x, faces.data[i].pos.y,
                           faces.data[i].pos.width, faces.data[i].pos.height);
        res.name = "Unknown";
        res.confidence = 0.0f;
        res.isKnown = false;

        // 如果数据库为空，直接返回 Unknown
        if (db_records.empty()) {
            results.push_back(res);
            continue;
        }

        // 提取特征点
        std::vector<SeetaPointF> points(landmarker->number());
        landmarker->mark(seeta_image, faces.data[i].pos, points.data());

        // 提取特征向量
        std::vector<float> query_features(recognizer->GetExtractFeatureSize());
        recognizer->Extract(seeta_image, points.data(), query_features.data());

        // 在数据库中比对
        float best_similarity = 0.0f;
        FaceRecord best_match;

        for (const auto& record : db_records) {
            float similarity = recognizer->CalculateSimilarity(query_features.data(), record.feature.data());
            if (similarity > best_similarity) {
                best_similarity = similarity;
                best_match = record;
            }
        }

        // 判断是否超过阈值
        if (best_similarity > threshold) {
            res.name = best_match.person_id;
            res.confidence = best_similarity;
            res.isKnown = true;
            // res.phone = best_match.phone;   // 可选：附加信息
            // res.gender = best_match.gender; // 可选：附加信息
        } else {
            res.confidence = best_similarity; // 记录最高分，哪怕未通过阈值
        }

        results.push_back(res);
    }

    return results;
}


// // 注册人脸
// bool FaceEngine::addFace(const cv::Mat& image, const std::string& person_id) {
//     if (image.empty()) return false;
//
//     SeetaImageData seeta_image;
//     seeta_image.width = image.cols;
//     seeta_image.height = image.rows;
//     seeta_image.channels = image.channels();
//     seeta_image.data = image.data;
//
//     std::vector<float> features;
//     if (extractFeaturesInternal(seeta_image, features)) {
//         face_database[person_id] = features;
//         return true;
//     }
//     return false;
// }
//
// // 核心：检测并识别当前帧
// std::vector<FaceResult> FaceEngine::detectAndRecognize(const cv::Mat& frame, float threshold) {
//     std::vector<FaceResult> results;
//     if (frame.empty()) return results;
//
//     SeetaImageData seeta_image;
//     seeta_image.width = frame.cols;
//     seeta_image.height = frame.rows;
//     seeta_image.channels = frame.channels();
//     seeta_image.data = frame.data;
//
//     // 1. 检测所有人脸
//     auto faces = detector->detect(seeta_image);
//
//     // 2. 遍历每个人脸进行识别
//     for (int i = 0; i < faces.size; ++i) {
//         FaceResult res;
//         res.box = cv::Rect(faces.data[i].pos.x, faces.data[i].pos.y, faces.data[i].pos.width, faces.data[i].pos.height);
//         res.name = "Unknown";
//         res.confidence = 0.0f;
//         res.isKnown = false;
//
//         // 如果数据库为空，直接返回Unknown
//         if (face_database.empty()) {
//             results.push_back(res);
//             continue;
//         }
//
//         // 提取特征点
//         std::vector<SeetaPointF> points(landmarker->number());
//         landmarker->mark(seeta_image, faces.data[i].pos, points.data());
//
//         // 提取特征值
//         std::vector<float> query_features(recognizer->GetExtractFeatureSize());
//         recognizer->Extract(seeta_image, points.data(), query_features.data());
//
//         // 在数据库中比对
//         float best_similarity = 0.0f;
//         std::string best_match = "";
//
//         for (const auto& [person_id, db_features] : face_database) {
//             float similarity = recognizer->CalculateSimilarity(query_features.data(), db_features.data());
//             if (similarity > best_similarity) {
//                 best_similarity = similarity;
//                 best_match = person_id;
//             }
//         }
//
//         if (best_similarity > threshold) {
//             res.name = best_match;
//             res.confidence = best_similarity;
//             res.isKnown = true;
//         } else {
//             res.confidence = best_similarity; // 记录最高分，哪怕未通过阈值
//         }
//
//         results.push_back(res);
//     }
//     return results;
// }
//
// // 数据库加载
// bool FaceEngine::loadDatabase(const std::string& filename) {
//     std::ifstream infile(filename, std::ios::binary);
//     if (!infile.is_open()) return false;
//
//     face_database.clear();
//     size_t face_count;
//     infile.read(reinterpret_cast<char*>(&face_count), sizeof(size_t));
//
//     for (size_t i = 0; i < face_count; ++i) {
//         size_t id_length;
//         infile.read(reinterpret_cast<char*>(&id_length), sizeof(size_t));
//         std::string person_id(id_length, '\0');
//         infile.read(&person_id[0], id_length);
//
//         size_t feature_size;
//         infile.read(reinterpret_cast<char*>(&feature_size), sizeof(size_t));
//         std::vector<float> features(feature_size);
//         infile.read(reinterpret_cast<char*>(features.data()), feature_size * sizeof(float));
//
//         face_database[person_id] = features;
//     }
//     infile.close();
//     return true;
// }
//
// // 数据库保存
// bool FaceEngine::saveDatabase(const std::string& filename) {
//     std::ofstream outfile(filename, std::ios::binary);
//     if (!outfile.is_open()) return false;
//
//     size_t face_count = face_database.size();
//     outfile.write(reinterpret_cast<const char*>(&face_count), sizeof(size_t));
//
//     for (const auto& [person_id, features] : face_database) {
//         size_t id_length = person_id.length();
//         outfile.write(reinterpret_cast<const char*>(&id_length), sizeof(size_t));
//         outfile.write(person_id.c_str(), id_length);
//
//         size_t feature_size = features.size();
//         outfile.write(reinterpret_cast<const char*>(&feature_size), sizeof(size_t));
//         outfile.write(reinterpret_cast<const char*>(features.data()), feature_size * sizeof(float));
//     }
//     outfile.close();
//     return true;
// }
//
// bool FaceEngine::deleteFace(const std::string& person_id) {
//     return face_database.erase(person_id) > 0;
// }
//
// std::map<std::string, std::vector<float>> FaceEngine::getDatabase() const {
//     return face_database;
// }