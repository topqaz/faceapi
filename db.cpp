//
// Created by root on 2026/1/21.
//

#include "db.h"
#include <iostream>

FaceDB::FaceDB() {}

FaceDB::~FaceDB() {
    close();
}

bool FaceDB::open(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "[FaceDB] open failed\n";
        return false;
    }
    return createTable();
}

void FaceDB::close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool FaceDB::createTable() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS faces (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            person_id TEXT UNIQUE NOT NULL,
            phone TEXT,
            gender TEXT,
            feature BLOB NOT NULL
        );
    )";

    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[FaceDB] create table failed: " << err << std::endl;
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool FaceDB::addFace(const FaceRecord& record) {
    const char* sql =
        "INSERT OR REPLACE INTO faces (person_id, phone, gender, feature) "
        "VALUES (?, ?, ?, ?)";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, record.person_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, record.phone.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, record.gender.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 4,
        record.feature.data(),
        record.feature.size() * sizeof(float),
        SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool FaceDB::deleteFace(const std::string& person_id) {
    const char* sql = "DELETE FROM faces WHERE person_id = ?";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, person_id.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool FaceDB::getAllFaces(std::vector<FaceRecord>& records) {
    records.clear();
    const char* sql = "SELECT person_id, phone, gender, feature FROM faces";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FaceRecord rec;
        rec.person_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        rec.phone     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.gender    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        const float* feat =
            reinterpret_cast<const float*>(sqlite3_column_blob(stmt, 3));
        int bytes = sqlite3_column_bytes(stmt, 3);

        rec.feature.assign(feat, feat + bytes / sizeof(float));
        records.push_back(std::move(rec));
    }
    sqlite3_finalize(stmt);
    return true;
}