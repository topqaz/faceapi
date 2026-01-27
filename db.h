#pragma once
#include <string>
#include <vector>
#include <sqlite3.h>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>
struct FaceRecord {
    std::string person_id;
    std::string phone;
    std::string gender;
    std::vector<float> feature;
};

class FaceDB {
public:
    FaceDB();
    ~FaceDB();

    bool open(const std::string& db_path);
    void close();

    bool addFace(const FaceRecord& record);
    bool deleteFace(const std::string& person_id);
    bool getAllFaces(std::vector<FaceRecord>& records);

private:
    sqlite3* db = nullptr;
    bool createTable();
};