find_package(assimp REQUIRED)
find_package(cxxopts REQUIRED)
find_package(spdlog REQUIRED)

add_executable(uvula cli.cpp)
target_link_libraries(uvula PUBLIC libuvula assimp::assimp cxxopts::cxxopts spdlog::spdlog)
