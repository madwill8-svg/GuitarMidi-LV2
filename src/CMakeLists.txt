set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)
configure_file(include/config.hpp.in include/config.hpp)
include_directories(${PROJECT_BINARY_DIR}/src/include)
include_directories(/usr/include)
include_directories(include)
find_package(PkgConfig REQUIRED)

find_package(OpenMP REQUIRED)
pkg_check_modules(Pango REQUIRED pango)

set(SRC
    guitarmidi.cpp
    noteclassifier.cpp
    midioutput.cpp
    fretboard.cpp
    harmonicgroup.cpp
    logging.cpp
    stringfingerprint.cpp)

add_library(guitarmidi SHARED ${SRC})
if(WITH_AUBIO=On)
message("Building with onset support")
target_link_libraries(guitarmidi dsp aubio ${Pango_LIBRARIES})
else()
message("Building without onset support")
target_link_libraries(guitarmidi PUBLIC dsp ${Pango_LIBRARIES})
target_link_libraries(guitarmidi PUBLIC OpenMP::OpenMP_CXX)
endif()

if(BUILD_FINGERPRINT_TESTS)
    add_executable(stringfingerprint_test
        ../tests/stringfingerprint_test.cpp
        stringfingerprint.cpp)
    target_include_directories(stringfingerprint_test PRIVATE include)
    add_test(NAME stringfingerprint_test COMMAND stringfingerprint_test)
endif()

install(TARGETS guitarmidi
        DESTINATION guitarmidi.lv2)

install(FILES ../manifest.ttl ../guitarmidi.ttl
        DESTINATION guitarmidi.lv2)
