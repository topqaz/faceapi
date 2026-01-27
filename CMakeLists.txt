cmake_minimum_required(VERSION 3.16)

# =========================
# CMake Policy
# =========================
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(FaceProject LANGUAGES CXX)

# =========================
# 1. 基础配置
# =========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

# =========================
# 2. 系统依赖
# =========================
find_package(Threads REQUIRED)

find_package(Boost REQUIRED COMPONENTS system)
find_package(SQLite3 REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(Crow CONFIG REQUIRED)

# =========================
# 3. OpenCV
# =========================
if(NOT OpenCV_DIR AND UNIX)
    # 你自己编译并 install 的 OpenCV
    set(OpenCV_DIR
            "/root/opencv_4.12_static/build_release/opencv_install/lib/cmake/opencv4"
            CACHE PATH "OpenCV root"
    )
endif()

find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR} NO_DEFAULT_PATH)

message(STATUS "OpenCV Version: ${OpenCV_VERSION}")
message(STATUS "OpenCV Include: ${OpenCV_INCLUDE_DIRS}")

# =========================
# 4. SeetaFace
# =========================
set(SEETA_ROOT "${CMAKE_SOURCE_DIR}/SeetaFace/index")

set(SEETA_INCLUDE_DIR
        ${SEETA_ROOT}/build/include
        ${SEETA_ROOT}/build/include/seeta
)

set(SEETA_LIB_DIR
        ${SEETA_ROOT}/build/lib/arm64
)

if(NOT EXISTS "${SEETA_LIB_DIR}")
    message(FATAL_ERROR "SeetaFace lib dir not found: ${SEETA_LIB_DIR}")
endif()

add_library(SeetaSDK INTERFACE)

target_include_directories(SeetaSDK INTERFACE
        ${SEETA_INCLUDE_DIR}
)

target_link_directories(SeetaSDK INTERFACE
        ${SEETA_LIB_DIR}
)

target_link_libraries(SeetaSDK INTERFACE
        SeetaFaceDetector600
        SeetaFaceLandmarker600
        SeetaFaceRecognizer610
        tennis
        SeetaAuthorize
)

# =========================
# 5. 可执行文件
# =========================
add_executable(FaceProject
        main.cpp
        FaceEngine.cpp
        auth.cpp
        db.cpp
)

target_include_directories(FaceProject PRIVATE
        ${OpenCV_INCLUDE_DIRS}
)

# =========================
# 6. 链接库（核心）
# =========================
target_link_libraries(FaceProject
        PRIVATE
        SeetaSDK
        ${OpenCV_LIBS}
        Boost::system
        Crow::Crow
        jwt-cpp::jwt-cpp
        SQLite::SQLite3
)

# =========================
# 7. RPATH（🔥 可移植关键）
# =========================
if(UNIX AND NOT APPLE)
    set_target_properties(FaceProject PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            BUILD_RPATH "\$ORIGIN/lib"
            INSTALL_RPATH "\$ORIGIN/lib"
    )
endif()

# =========================
# 8. 安装（用于打包）
# =========================
install(TARGETS FaceProject
        RUNTIME DESTINATION .
)

install(DIRECTORY
        ${SEETA_LIB_DIR}/
        DESTINATION lib
        FILES_MATCHING PATTERN "*.so*"
)