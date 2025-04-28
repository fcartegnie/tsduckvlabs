cmake_minimum_required(VERSION 3.24) # LINK_LIBRARY:WHOLE_ARCHIVE

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/tsVersion.h TSVERSION_H REGEX "^#define TS_")
set(TS_VERSION_MAJOR 0)
set(TS_VERSION_MINOR 0)
set(TS_COMMIT 0)
foreach(define ${TSVERSION_H})
    if (define MATCHES "#define TS_VERSION_MAJOR")
        string(REGEX REPLACE "#define TS_VERSION_MAJOR[ \t\r\n]*" "" TS_VERSION_MAJOR ${define})
    elseif (define MATCHES "#define TS_VERSION_MINOR")
        string(REGEX REPLACE "#define TS_VERSION_MINOR[ \t\r\n]*" "" TS_VERSION_MINOR ${define})
    elseif (define MATCHES "#define TS_COMMIT")
        string(REGEX REPLACE "#define TS_COMMIT[ \t\r\n]*" "" TS_COMMIT ${define})
    endif()
endforeach()

project(tsduck
    VERSION ${TS_VERSION_MAJOR}.${TS_VERSION_MINOR}.${TS_COMMIT}
    LANGUAGES CXX)

include(FeatureSummary)

if(WIN32)
    # TODO: call scripts/install-wintools/install-dektec.ps1 to install the actual SDK
    # It will probably not work in mingw-w64 as it's a C++ library compatible with the MSVC C++ runtime
    set(NODTAPI_DEFAULT ON)
endif()

option(TSDUCK_TOOLS "Build tsduck tools" ON)

option(NOPCSC "No smartcard support, remove dependency to pcsc-lite")
option(NOGITHUB "No version check, no download, no upgrade from GitHub (for distro packaging)")
option(NODTAPI "No Dektec device support, remove dependency to DTAPI" ${NODTAPI_DEFAULT})
option(NOHIDES "No HiDes device support")
option(NOVATEK "No Vatek-based device support")
option(NOEDITLINE "No interactive line editing, remove dependency to libedit")
option(NOCURL "No HTTP support, remove dependency to libcurl")
option(NOSRT "No SRT support, remove dependency to libsrt")
option(NORIST "No RIST support, remove dependency to librist")
option(NODEPRECATE "Do not flag legacy methods as deprecated")
option(NOOPENSSL "No cryptographic support, remove dependency to openssl")
option(NOZLIB "Don't use zlib, use embedded \"Small Deflate\" instead, remove dependency to zlib")

option(NOVLC "Remove pieces of code unused in VLC" ON)

option(BUILD_TESTING "Build tests" OFF)

set(BITRATE_FORMAT "float" CACHE STRING "bitrate representation (float fraction integer fixed decimals=xx)")
if(BITRATE_FORMAT STREQUAL "fraction")
    set(BITRATE_FORMAT_FLAG -DTS_BITRATE_FRACTION=1)
elseif(BITRATE_FORMAT STREQUAL "integer")
    set(BITRATE_FORMAT_FLAG -DTS_BITRATE_INTEGER=1)
elseif(BITRATE_FORMAT STREQUAL "float")
    set(BITRATE_FORMAT_FLAG -DTS_BITRATE_FLOAT=1)
elseif(BITRATE_FORMAT STREQUAL "fixed")
    set(BITRATE_FORMAT_FLAG -DTS_BITRATE_FIXED=1)
elseif(BITRATE_FORMAT MATCHES "^decimals=[0-9]+$")
    string(SUBSTRING BITRATE_FORMAT 9 -1 BITRATE_FORMAT_DECIMAL)
    set(BITRATE_FORMAT_FLAG -DTS_BITRATE_DECIMALS=${BITRATE_FORMAT_DECIMAL})
else()
    # fallback default mode
    set(BITRATE_FORMAT_FLAG -DTS_BITRATE_FRACTION=1)
endif()

if(NOT BUILD_SHARED_LIBS)
    # if(NOT WIN32 OR MINGW)
    #     set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -static")
    #     set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} -static")
    # endif()
    if(LINUX OR BSD)
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,noexecstack")
        set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,noexecstack")
    endif()
endif()

set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(POSITION_INDEPENDENT_CODE ON)

include(CheckCXXCompilerFlag)
list(APPEND CHECK_CXX_COMPILER_FLAGS -fno-strict-aliasing -funroll-loops -fomit-frame-pointer)
# disabled as it doesn't build on Windows
# list(APPEND CHECK_CXX_COMPILER_FLAGS -Werror)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(NOT WIN32)
        # disabled on Windows as it's too noisy
        list(APPEND CHECK_CXX_COMPILER_FLAGS -Weverything)
    endif()
    list(APPEND CHECK_CXX_COMPILER_FLAGS -Wno-c++98-compat-pedantic)
    if(APPLE AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12)
        # On macOS, it is normal to include from /usr/local/include since some libraries come from Homebrew.
        # Starting with clang 12, this generates a warning we need to disable. However, this disable option
        # generates an error with previous versions. And we cannot disable this warning inside the code since
        # this is a command-line-level error. So, we must check the version here...
        list(APPEND CHECK_CXX_COMPILER_FLAGS -Wno-poison-system-directories)
    endif()
    if (NOT MINGW)
        # warning: non-portable path to file '<Windows.h>' with the Windows SDK
        list(APPEND CHECK_CXX_COMPILER_FLAGS -Wno-nonportable-system-include-path)
    endif()
else()
    list(APPEND CHECK_CXX_COMPILER_FLAGS -Wall -Wextra -Wformat-nonliteral -Wformat-security -Wswitch-default -Wuninitialized
        -Wfloat-equal -Wundef -Wpointer-arith -Woverloaded-virtual -Wctor-dtor-privacy
        -Wnon-virtual-dtor -Wsign-promo -Wzero-as-null-pointer-constant -Wstrict-null-sentinel)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GCC")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 5)
            list(APPEND CHECK_CXX_COMPILER_FLAGS -Wshadow -Wpedantic -Weffc++)
        endif()
        if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6)
            list(APPEND CHECK_CXX_COMPILER_FLAGS -Wsuggest-override)
        endif()
    endif()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        list(APPEND CHECK_CXX_COMPILER_FLAGS -Wno-psabi)
    endif()
endif()
list(APPEND CHECK_CXX_COMPILER_FLAGS -fstack-protector-all)
if(NOT BUILD_SHARED_LIBS)
    list(APPEND CHECK_CXX_COMPILER_FLAGS -static-libstdc++)
    list(APPEND CHECK_CXX_COMPILER_FLAGS -static-libgcc)
endif()

check_cxx_compiler_flag("-Werror=unused-command-line-argument" clang_unused_command)
if (clang_unused_command)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror=unused-command-line-argument")
endif()

foreach(num ${CHECK_CXX_COMPILER_FLAGS})
    string(REPLACE "-" "_" flag_under ${num})
    string(REPLACE "+" "p" flag_plus ${flag_under})
    string(PREPEND flag_plus "TSDUCK_CXXFLAG")
    check_cxx_compiler_flag(${num} ${flag_plus})
    if(${flag_plus})
        list(APPEND USE_CXX_COMPILER_FLAGS ${num})
    endif()
endforeach()
add_compile_options(${USE_CXX_COMPILER_FLAGS})


file(GLOB_RECURSE TSDUCK_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/*.h)
list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+/private")


file(GLOB TSPLUGINS LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src/tsplugins "${CMAKE_CURRENT_SOURCE_DIR}/src/tsplugins/*\.cpp")


file(GLOB_RECURSE tsduck_SOURCES LIST_DIRECTORIES true 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/*/*\.cpp")
# Keep only the C++ files
list(FILTER tsduck_SOURCES INCLUDE REGEX "\.cpp$")
list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/.+/")
# TODO add java support
list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/java/")
# Remove all OS specific system files
list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.*/bsd/.*")
list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.*/unix/.*")
list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.*/linux/.*")
list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.*/mac/.*")
list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.*/windows/.*")

# Add OS specific system files
if(UNIX)
    file(GLOB tsduck_system_SOURCES LIST_DIRECTORIES true 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/unix/*\.cpp")
    list(APPEND tsduck_SOURCES ${tsduck_system_SOURCES})
else()
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+/unix")
endif()
if(LINUX)
    file(GLOB tsduck_system_SOURCES LIST_DIRECTORIES true 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/linux/*\.cpp")
    list(APPEND tsduck_SOURCES ${tsduck_system_SOURCES})
else()
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+/linux")
endif()
if(APPLE)
    file(GLOB tsduck_system_SOURCES LIST_DIRECTORIES true 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/mac/*\.cpp")
    list(APPEND tsduck_SOURCES ${tsduck_system_SOURCES})
else()
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+/mac")
endif()
if(WIN32)
    file(GLOB tsduck_system_SOURCES LIST_DIRECTORIES true 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/windows/*\.cpp")
    list(APPEND tsduck_SOURCES ${tsduck_system_SOURCES})
else()
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+/windows")
endif()

# Add OS specific network files
if(NOT NOVLC)
if(WIN32)
    file(GLOB tsduck_network_SOURCES LIST_DIRECTORIES true 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/windows/*\.cpp")
    list(APPEND tsduck_SOURCES ${tsduck_network_SOURCES})
elseif(UNIX)
    file(GLOB tsduck_network_SOURCES LIST_DIRECTORIES true 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/unix/*\.cpp")
    list(APPEND tsduck_SOURCES ${tsduck_network_SOURCES})
endif()
endif()

file(GLOB tsduck_transport_SOURCES LIST_DIRECTORIES true 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/*\.cpp")
list(APPEND tsduck_SOURCES ${tsduck_transport_SOURCES})
if(NOT NOVLC)
if(BSD)
    file(GLOB tsduck_broadcast_SOURCES LIST_DIRECTORIES true 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/bsd/*\.cpp")
    list(APPEND tsduck_SOURCES ${tsduck_broadcast_SOURCES})
endif()
if(LINUX)
    file(GLOB tsduck_broadcast_SOURCES LIST_DIRECTORIES true 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/linux/*\.cpp")
    list(APPEND tsduck_SOURCES ${tsduck_broadcast_SOURCES})
endif()
if(APPLE)
    file(GLOB tsduck_broadcast_SOURCES LIST_DIRECTORIES true 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/mac/*\.cpp")
    list(APPEND tsduck_SOURCES ${tsduck_broadcast_SOURCES})
endif()
if(WIN32)
    file(GLOB tsduck_broadcast_SOURCES LIST_DIRECTORIES true 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/windows/*\.cpp")
    list(APPEND tsduck_SOURCES ${tsduck_broadcast_SOURCES})
endif()
endif()

if(NOT NOHIDES)
    if(BSD)
        file(GLOB tsduck_hides_SOURCES LIST_DIRECTORIES true 
            "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/bsd/*\.cpp")
        list(APPEND tsduck_SOURCES ${tsduck_hides_SOURCES})
    endif()
    if(LINUX)
        file(GLOB tsduck_hides_SOURCES LIST_DIRECTORIES true 
            "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/linux/*\.cpp")
        list(APPEND tsduck_SOURCES ${tsduck_hides_SOURCES})
    endif()
    if(APPLE)
        file(GLOB tsduck_hides_SOURCES LIST_DIRECTORIES true 
            "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/mac/*\.cpp")
        list(APPEND tsduck_SOURCES ${tsduck_hides_SOURCES})
    endif()
    if(WIN32)
        file(GLOB tsduck_hides_SOURCES LIST_DIRECTORIES true 
            "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/windows/*\.cpp")
        list(APPEND tsduck_SOURCES ${tsduck_hides_SOURCES})
    endif()
endif()

if(NOVLC)
    list(REMOVE_ITEM tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/cas/tsKeyTable.cpp")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsPcap.*")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsURL.*")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsWeb.*")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/.*/tsWeb.*")
    # message(FATAL_ERROR "tsduck_SOURCES=${tsduck_SOURCES}")
endif()



file(GLOB_RECURSE tsduck_PUBLIC_HEADERS LIST_DIRECTORIES true 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/*\.h")
list(FILTER tsduck_PUBLIC_HEADERS INCLUDE REGEX "\.h$")
set(tsduck_PRIVATE_HEADERS ${tsduck_PUBLIC_HEADERS})
list(FILTER tsduck_PRIVATE_HEADERS INCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+/private")
list(TRANSFORM tsduck_PRIVATE_HEADERS REPLACE "/[^/]*\.h$" "" OUTPUT_VARIABLE tsduck_PRIVATE_INCLUDES)
list(REMOVE_DUPLICATES tsduck_PRIVATE_INCLUDES)

# remove OS specific broadcast files
list(FILTER tsduck_PUBLIC_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/.+/")
# remove OS specific hides files
list(FILTER tsduck_PUBLIC_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/.+/")

list(TRANSFORM tsduck_PUBLIC_HEADERS REPLACE "/[^/]*\.h$" "" OUTPUT_VARIABLE tsduck_PUBLIC_INCLUDES)
list(TRANSFORM tsduck_SOURCES REPLACE "/[^/]*\.cpp$" "" OUTPUT_VARIABLE tsduck_SOURCE_INCLUDES)
list(APPEND tsduck_PUBLIC_INCLUDES ${tsduck_SOURCE_INCLUDES})
list(REMOVE_DUPLICATES tsduck_PUBLIC_INCLUDES)
list(FILTER tsduck_PUBLIC_INCLUDES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+/private")
list(APPEND tsduck_PUBLIC_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck")



# extra packages
find_package(PkgConfig)

if(WIN32 AND NOT MSVC)
    # MSVC compatible compilers use "#pragma comment" to add libraries
    list(APPEND LIBTSDUCK_LINK_LIBRARIES bcrypt ws2_32 iphlpapi wininet userenv winmm ksuser strmiids wmcodecdspuuid psapi shell32 quartz)
endif()

if(NODEPRECATE)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_DEPRECATE=1)
endif()

if(NOT NOPCSC)
    pkg_check_modules(PCSCLITE QUIET IMPORTED_TARGET pcsclite)
endif()
if(NOT PCSCLITE_FOUND)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_PCSC=1)
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/cas/tsPCSC.+")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/cas/tsPCSC.+")
else()
    list(APPEND LIBTSDUCK_LINK_LIBRARIES PkgConfig::PCSCLITE)
    list(APPEND LIBTSDUCK_LINK_LIBRARIES winscard)
endif()

if(NOGITHUB OR NOVLC)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_GITHUB=1)
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsGitHub.+")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsGitHub.+")
endif()

if(NOHIDES)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_HIDES=1)
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/")
else()
    if(BSD)
        list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/[^bsd/]")
        list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/[^bsd/]")
    endif()
    if(LINUX)
        list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/[^linux/]")
        list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/[^linux/]")
    endif()
    if(APPLE)
        list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/[^mac/]")
        list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/[^mac/]")
    endif()
    if(WIN32)
        list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/[^windows/]")
        list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/[^windows/]")
    endif()
endif()

if(NOEDITLINE)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_EDITLINE=1)
endif()

set(EDITLINE_FOUND 0)
if(NOT NOEDITLINE)
    pkg_check_modules(EDITLINE QUIET IMPORTED_TARGET edit)
endif()
if(NOT EDITLINE_FOUND)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_EDITLINE=1)
else()
    list(APPEND LIBTSDUCK_LINK_LIBRARIES PkgConfig::EDITLINE)
endif()

set(CURL_FOUND 0)
if(NOT NOCURL)
    if(NOT BUILD_SHARED_LIBS)
        set(CURL_NO_CURL_CMAKE ON)
        set(CURL_USE_STATIC_LIBS TRUE)
    endif()
    find_package(CURL QUIET)
endif()
if(NOT CURL_FOUND)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_CURL=1)
else()
    list(APPEND LIBTSDUCK_LINK_LIBRARIES CURL::libcurl)
endif()

set(RIST_FOUND 0)
if(NOT NORIST)
    pkg_check_modules(RIST QUIET IMPORTED_TARGET rist)
endif()
if(NOT RIST_FOUND)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_RIST=1)
    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+tsRIST.+")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+tsRIST.+")
else()
    list(APPEND LIBTSDUCK_LINK_LIBRARIES PkgConfig::RIST)
endif()

set(SRT_FOUND 0)
if(NOT NOSRT)
    find_package(SRT QUIET)
endif()
if(NOT SRT_FOUND)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_SRT=1)
    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+tsSRT.+")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+tsSRT.+")
else()
    list(APPEND LIBTSDUCK_LINK_LIBRARIES SRT)
endif()

set(OpenSSL_FOUND 0)
if(NOT NOOPENSSL AND NOT WIN32 AND BUILD_SHARED_LIBS) # avoid linking problems between static and dynamic libraries for now
    find_package(OpenSSL QUIET)
endif()
if(NOT OpenSSL_FOUND)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_OPENSSL=1)
else()
    list(APPEND LIBTSDUCK_LINK_LIBRARIES OpenSSL::Crypto)
endif()

if(NOT NODTAPI)
    if(WIN32)
        message(FATAL_ERROR "DTAPI currently not supported for Windows, use -DNODTAPI=ON")
    endif()
    include(FetchContent REQUIRED)
    if(WIN32)
        set(DTAPI_URL    "https://www.dektec.com/products/SDK/DTAPI/Downloads/WinSDK_v2025.01.0.zip")
        set(DTAPI_SHA512 "04d048ad9ed2098c38586d6695498e5ec3da9fa835d4e0602343f1effe21fdca0efbec1579b7a39421e49a12edc63f6c86a5dbf48cf1f2b8aba2df778e368302")
    elseif(LINUX)
        set(DTAPI_URL    "https://www.dektec.com/products/SDK/DTAPI/Downloads/LinuxSDK_v2025.01.0.tar.gz")
        set(DTAPI_SHA512 "916388765ae2e8cf9416464ca4cd14c055858cea55a86ecb9e2bfc29ead193e871aa380b9648f795a37a13e328faff7dffd1016eacf8173f14b7311bd528fee6")
    endif()
    FetchContent_Declare(
        DTAPI
        DOWNLOAD_EXTRACT_TIMESTAMP ON
        URL      ${DTAPI_URL}
        URL_HASH SHA512=${DTAPI_SHA512}
    )
    FetchContent_MakeAvailable(DTAPI)
    list(APPEND tsduck_PRIVATE_INCLUDES ${FETCHCONTENT_BASE_DIR}/dtapi-src/DTAPI/Include)
    list(APPEND LIBTSDUCK_LINK_LIBRARIES ${FETCHCONTENT_BASE_DIR}/dtapi-src/DTAPI/Lib/GCC7.5.0/CDTAPI64.a)
else()
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_DTAPI=1)
    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dektec/")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dektec/")
endif()


set(NOVATEK_FOUND 0)
if(NOT NOVATEK)
    find_package(vatek QUIET)
endif()
if(NOT NOVATEK_FOUND)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_VATEK=1)
    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/vatek/")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/vatek/")
else()
    list(APPEND LIBTSDUCK_LINK_LIBRARIES vatek)
endif()

set(ZLIB_FOUND 0)
if(NOT NOZLIB)
    find_package(ZLIB QUIET)
endif()
if(NOT ZLIB_FOUND)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_ZLIB=1)
else()
    list(APPEND LIBTSDUCK_LINK_LIBRARIES ZLIB::ZLIB)
endif()


# TODO Use CONFIGURE_DEPENDS to update live
if(NOOPENSSL)
    list(REMOVE_ITEM TSPLUGINS "tsplugin_aes.cpp")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_descrambler.cpp")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_scrambler.cpp")
endif()
if(NOHIDES)
    list(REMOVE_ITEM TSPLUGINS "tsplugin_hides.cpp")
endif()
if(NOVLC)
    # list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_WEB=1)
    # issues with DEFAULT_POLL_INTERVAL / DEFAULT_MIN_STABLE_DELAY dll export in gcc
    list(REMOVE_ITEM TSPLUGINS "tsplugin_eitinject.cpp")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_spliceinject.cpp")
    # issue with DEFAULT_MIN_WAIT  dll export in gcc
    list(REMOVE_ITEM TSPLUGINS "tsplugin_regulate.cpp")

    list(REMOVE_ITEM TSPLUGINS "tsplugin_nitscan.cpp")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_pcap.cpp")

    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hls/")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hls/")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+tshls.+")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+tshls.+")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+HTTP.+")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+HTTP.+")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+tsDirectShow.+")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+tsDirectShow.+")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+[tT]eletext.+")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/.+[tT]eletext.+")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/python/")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/python/")

    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/analysis/tsTSAnalyzer.+")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/analysis/tsTSAnalyzer.+")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_analyze.cpp")

    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/")
    # list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/.*tsTuner.+")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/.*tsTuner.+")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsTSScanner.+")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsTSScanner.+")
    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/cas/")
    # list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/cas/")
    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/charset/")
    # list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/charset/")
    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/")
    # list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/.+/")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/.+/")

    # list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsNetworkNameDescriptor.cpp")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_nit.cpp")

    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDataBroadcastIdDescriptor.cpp")
    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsServiceDescriptor.cpp")
    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSatelliteDeliverySystemDescriptor.cpp")
    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsAbstractDeliverySystemDescriptor.cpp")
    # list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsServiceListDescriptor.cpp")
    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsStreamIdentifierDescriptor.cpp")
    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsLocalTimeOffsetDescriptor.cpp")
    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBAC3Descriptor.cpp")
    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBEnhancedAC3Descriptor.cpp")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_dvb.cpp")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_scrambler.cpp")

    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/eacem/")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/eacem/")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/eutelsat/")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/eutelsat/")
    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsISDBAccessControlDescriptor.cpp")
    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_isdbinfo.cpp")

    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/")

    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/nordig/")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/nordig/")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/scte/")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/scte/")
    file(GLOB_RECURSE tsduck_DESCRIPTOR_SOURCES LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/*/*Channel*.cpp")
    # message(FATAL_ERROR "tsduck_DESCRIPTOR_SOURCES=${tsduck_DESCRIPTOR_SOURCES}")
    list(APPEND tsduck_SOURCES ${tsduck_DESCRIPTOR_SOURCES})
    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsCADescriptor.cpp")
    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsISO639LanguageDescriptor.cpp")
    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsRegistrationDescriptor.cpp") # needed by pmt
    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/scte/tsCueIdentifierDescriptor.cpp") # needed by pmt
    list(APPEND tsduck_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMetadataDescriptor.cpp") # needed by tests

    list(REMOVE_ITEM TSPLUGINS "tsplugin_splicemonitor.cpp")


    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/mpe/")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/mpe/")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_mpe.cpp")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_mpeinject.cpp")

    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/t2mi/")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/t2mi/")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_t2mi.cpp")

    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/atsc/")
    # list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/atsc/")
    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsSAT.+")
    # list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsSAT.+")
    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/isdb/")
    # list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/isdb/")
    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/scte/")
    # list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/scte/")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_eit.cpp")

    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/")
    # list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/")
    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/encap/")
    # list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/encap/")
    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/epg/")
    # list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/epg/")
    list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSFuzzing.+")
    list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSFuzzing.+")
    list(REMOVE_ITEM TSPLUGINS "tsplugin_fuzz.cpp")

    # list(FILTER tsduck_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/timing/")
    # list(FILTER TSDUCK_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/timing/")
endif()

if(NOT NOVLC)
set(TSDUCK_H_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${TSDUCK_H_DIRECTORY})
find_package(Python REQUIRED COMPONENTS Interpreter)
add_custom_target(tsduck_header
    COMMENT "Building include/tsduck.h"
    DEPENDS ${TSDUCK_HEADERS}
    BYPRODUCTS ${TSDUCK_H_DIRECTORY}/tsduck.h
    COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-tsduck-header.py tsduck ${TSDUCK_H_DIRECTORY}/tsduck.h)
endif()


include(GNUInstallDirs)

if(BUILD_SHARED_LIBS)
    add_library(tsduck SHARED ${tsduck_SOURCES})
    target_compile_definitions(tsduck
        PUBLIC ${BITRATE_FORMAT_FLAG}
        PUBLIC ${LIBTSDUCK_CXXFLAGS_INCLUDES}
        PRIVATE TS_WINVER_FILETYPE=VFT_DLL
        PRIVATE _TSDUCKDLL_IMPL
        PUBLIC _TSDUCKDLL_USE)
    target_include_directories(tsduck 
        PUBLIC ${tsduck_PUBLIC_INCLUDES}
        PRIVATE ${tsduck_PRIVATE_INCLUDES})
    if(NOT NOVLC)
    # to include tsduck.h, not included in local code
    add_dependencies(tsduck tsduck_header)
    target_include_directories(tsduck PUBLIC $<BUILD_INTERFACE:${TSDUCK_H_DIRECTORY}>)
    endif()
    target_include_directories(tsduck PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
        
    target_link_libraries(tsduck PUBLIC ${LIBTSDUCK_LINK_LIBRARIES})

    set(tsduck_libs tsduck)

    install(TARGETS tsduck
        COMPONENT tsduck
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif(BUILD_SHARED_LIBS)

if(NOT BUILD_SHARED_LIBS)
    list(TRANSFORM TSPLUGINS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/src/tsplugins/ OUTPUT_VARIABLE tsplugin_static_SOURCES)
    list(APPEND tsduck_SOURCES ${tsplugin_static_SOURCES})

    add_library(tsducklib STATIC ${tsduck_SOURCES})
    target_compile_definitions(tsducklib
        PUBLIC ${BITRATE_FORMAT_FLAG}
        PUBLIC TSDUCK_STATIC
        PUBLIC ${LIBTSDUCK_CXXFLAGS_INCLUDES}
        PRIVATE TSDUCK_STATIC_LIBRARY)
    target_include_directories(tsducklib 
        PUBLIC ${tsduck_PUBLIC_INCLUDES}
        PRIVATE ${tsduck_PRIVATE_INCLUDES})
    if(NOT NOVLC)
    # to include tsduck.h, not included in local code
    add_dependencies(tsducklib tsduck_header)
    target_include_directories(tsducklib PUBLIC $<BUILD_INTERFACE:${TSDUCK_H_DIRECTORY}>)
    endif()
    target_include_directories(tsducklib PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_link_libraries(tsducklib PUBLIC ${LIBTSDUCK_LINK_LIBRARIES})
    
    set(tsduck_libs "$<LINK_LIBRARY:WHOLE_ARCHIVE,tsducklib>")

    install(TARGETS tsducklib
        COMPONENT tsduck
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()


if(BUILD_SHARED_LIBS)
    # plugins are built in tsp_static in static mode
    add_subdirectory(src/tsplugins)
endif(BUILD_SHARED_LIBS)
if(TSDUCK_TOOLS)
    add_subdirectory(src/tstools)
endif(TSDUCK_TOOLS)
if(BUILD_TESTING)
    add_subdirectory(src/utest)
endif()

feature_summary(INCLUDE_QUIET_PACKAGES WHAT ALL)
