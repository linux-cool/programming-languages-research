# DistFS 分布式文件系统构建配置
cmake_minimum_required(VERSION 3.20)

project(DistFS
    VERSION 1.0.0
    DESCRIPTION "High-performance Distributed File System"
    LANGUAGES C
)

# 设置C标准
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# 设置构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 编译选项
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -Wall -Wextra")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG -Wall -Wextra")

# 添加编译器特定选项
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-fstack-protector-strong -D_FORTIFY_SOURCE=2)
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fstack-protector-strong -D_FORTIFY_SOURCE=2)
endif()

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 查找依赖库
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# 查找可选依赖
pkg_check_modules(LIBLZ4 liblz4)
pkg_check_modules(LIBZSTD libzstd)
pkg_check_modules(OPENSSL openssl)

# 设置源文件目录 - 只包含已实现的文件
set(COMMON_SOURCES
    src/common/utils.c
    src/common/config.c
    src/common/log.c
    src/common/memory.c
    src/common/hash.c
)

set(NETWORK_SOURCES
    src/network/network.c
)

set(METADATA_SOURCES
    src/metadata/metadata_server.c
)

set(CLIENT_SOURCES
    src/client/client.c
)

# 创建静态库
add_library(distfs_common STATIC ${COMMON_SOURCES})
add_library(distfs_network STATIC ${NETWORK_SOURCES})
add_library(distfs_metadata STATIC ${METADATA_SOURCES})
add_library(distfs_client STATIC ${CLIENT_SOURCES})

# 链接依赖
target_link_libraries(distfs_common Threads::Threads)
target_link_libraries(distfs_network distfs_common Threads::Threads)
target_link_libraries(distfs_metadata distfs_common Threads::Threads)
target_link_libraries(distfs_client distfs_common distfs_network Threads::Threads)

# 添加可选依赖
if(LIBLZ4_FOUND)
    target_compile_definitions(distfs_common PRIVATE HAVE_LZ4)
    target_link_libraries(distfs_common ${LIBLZ4_LIBRARIES})
    target_include_directories(distfs_common PRIVATE ${LIBLZ4_INCLUDE_DIRS})
endif()

if(LIBZSTD_FOUND)
    target_compile_definitions(distfs_common PRIVATE HAVE_ZSTD)
    target_link_libraries(distfs_common ${LIBZSTD_LIBRARIES})
    target_include_directories(distfs_common PRIVATE ${LIBZSTD_INCLUDE_DIRS})
endif()

if(OPENSSL_FOUND)
    target_compile_definitions(distfs_common PRIVATE HAVE_OPENSSL)
    target_link_libraries(distfs_common ${OPENSSL_LIBRARIES})
    target_include_directories(distfs_common PRIVATE ${OPENSSL_INCLUDE_DIRS})
endif()

# 创建可执行文件
add_executable(distfs_metadata_server src/metadata/main.c)
target_link_libraries(distfs_metadata_server
    distfs_metadata distfs_network distfs_common)

add_executable(distfs_client_tool src/client/main.c)
target_link_libraries(distfs_client_tool
    distfs_client distfs_network distfs_common)

# 创建共享库
add_library(distfs SHARED
    ${COMMON_SOURCES}
    ${NETWORK_SOURCES}
    ${CLIENT_SOURCES}
)
target_link_libraries(distfs Threads::Threads)

# 设置库版本
set_target_properties(distfs PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# 测试配置
enable_testing()

# 基础功能测试
add_executable(test_basic tests/test_basic.c)
target_link_libraries(test_basic distfs_common distfs_network)
add_test(NAME test_basic COMMAND test_basic)

# 安装配置
install(TARGETS distfs distfs_metadata_server distfs_client_tool
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include/distfs)

# 配置文件安装
install(FILES 
    config/distfs.conf.example
    config/metadata.conf.example
    config/storage.conf.example
    DESTINATION etc/distfs
)

# 脚本安装
install(PROGRAMS
    scripts/start_cluster.sh
    scripts/stop_cluster.sh
    scripts/deploy.sh
    DESTINATION bin
)

# 文档安装
install(FILES
    README.md
    PROJECT_PLAN.md
    REQUIREMENTS.md
    DESTINATION share/doc/distfs
)

# 打包配置
set(CPACK_PACKAGE_NAME "distfs")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance Distributed File System")
set(CPACK_PACKAGE_VENDOR "DistFS Project")
set(CPACK_PACKAGE_CONTACT "distfs@example.com")

set(CPACK_GENERATOR "TGZ;DEB;RPM")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libpthread-stubs0-dev")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc, glibc-devel")

include(CPack)

# 代码覆盖率
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        add_compile_options(--coverage)
        add_link_options(--coverage)
    endif()
endif()

# 静态分析
option(ENABLE_STATIC_ANALYSIS "Enable static analysis" OFF)
if(ENABLE_STATIC_ANALYSIS)
    find_program(CLANG_TIDY clang-tidy)
    if(CLANG_TIDY)
        set(CMAKE_C_CLANG_TIDY ${CLANG_TIDY})
    endif()
endif()

# 内存检查
option(ENABLE_SANITIZER "Enable AddressSanitizer" OFF)
if(ENABLE_SANITIZER)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# 调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DISTFS_DEBUG)
    message(STATUS "Debug build enabled")
endif()

# 打印配置信息
message(STATUS "DistFS Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")

if(LIBLZ4_FOUND)
    message(STATUS "  LZ4 Support: Enabled")
else()
    message(STATUS "  LZ4 Support: Disabled")
endif()

if(LIBZSTD_FOUND)
    message(STATUS "  ZSTD Support: Enabled")
else()
    message(STATUS "  ZSTD Support: Disabled")
endif()

if(OPENSSL_FOUND)
    message(STATUS "  OpenSSL Support: Enabled")
else()
    message(STATUS "  OpenSSL Support: Disabled")
endif()
