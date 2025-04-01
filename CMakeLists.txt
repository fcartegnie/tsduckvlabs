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

option(TSDUCK_TOOLS "Build tsduck tools" ON)

option(NOPCSC "")
option(NOGITHUB "No version check, no download, no upgrade from GitHub." ON)
option(NODTAPI "")
option(NOHIDES "")
option(NOVATEK "")
option(NOEDITLINE "")
option(NOCURL "")
option(NOSRT "")
option(NORIST "")
option(NODEPRECATE "")

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
endif()

foreach(num ${CHECK_CXX_COMPILER_FLAGS})
    string(REPLACE "-" "_" flag_under ${num})
    string(PREPEND flag_under "TEST_FLAG")
    check_cxx_compiler_flag(${num} ${flag_under})
    if(${flag_under})
        list(APPEND USE_CXX_COMPILER_FLAGS ${num})
    endif()
endforeach()
add_compile_options(${USE_CXX_COMPILER_FLAGS})


file(GLOB_RECURSE TSDUCK_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/*.h)

set(TSDUCK_H_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${TSDUCK_H_DIRECTORY})
find_package(Python REQUIRED COMPONENTS Interpreter)
add_custom_target(tsduck_header
    COMMENT "Building include/tsduck.h"
    DEPENDS ${TSDUCK_HEADERS}
    COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-tsduck-header.py ${TSDUCK_H_DIRECTORY}/tsduck.h)

set(tsduck_base_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/algo/tsAbstractOutputStream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/algo/tsBoolPredicate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/algo/tsRingNode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/algo/tsUID.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/algo/tsWatchDog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/algo/tsWatchDogHandlerInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsCommandLine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsCommandLineHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsConfigFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsConfigSection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsDuckConfigFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsEditLine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsGitHubRelease.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsMain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsNamesFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsObject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsObjectRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app/tsVersionInfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/json/tsjson.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/json/tsjsonArray.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/json/tsjsonFalse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/json/tsjsonNull.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/json/tsjsonNumber.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/json/tsjsonObject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/json/tsjsonOutputArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/json/tsjsonRunningDocument.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/json/tsjsonString.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/json/tsjsonTrue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/json/tsjsonValue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsAbstractNetworkAddress.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsIPProtocols.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsIPUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsIPv4Address.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsIPv4AddressMask.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsIPv4Packet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsIPv4SocketAddress.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsIPv6Address.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsIPv6SocketAddress.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsMACAddress.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsPcapFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsPcapFilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsPcapStream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsRIST.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsSocket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsSRTSocket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsTCPConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsTCPServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsTCPSocket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsTelnetConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsUDPReceiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsUDPReceiverArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsUDPReceiverArgsList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsUDPSocket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsURL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsWebRequest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network/tsWebRequestArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/private/tsTimeConfigurationFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/report/tsAsyncReport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/report/tsAsyncReportArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/report/tsCerrReport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/report/tsErrCodeReport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/report/tsNullReport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/report/tsReport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/report/tsSeverity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsAbortInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsAbstractReadStreamInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsAbstractWriteStreamInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsApplicationSharedLibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsCOM.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsConsoleState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsEnvironment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsFatal.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsFileNameGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsFileUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsForkPipe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsInputRedirector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsInterruptHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsOutputPager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsOutputRedirector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsPagerArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsPolledFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsPollFiles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsPollFilesListener.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsSharedLibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsSysInfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsSystemMonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsSysUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsThread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsThreadAttributes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsTime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system/tsUserInterrupt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/text/tsGrid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/text/tsTextFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/text/tsTextParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/text/tsTLVSyntax.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types/tsAbstractNumber.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types/tsArgMix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types/tsBCD.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types/tsBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types/tsByteBlock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types/tsDisplayInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types/tsEnumeration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types/tsException.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types/tsIntegerUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types/tsMemory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types/tsStringifyInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types/tsUChar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types/tsUString.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml/tsxmlAttribute.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml/tsxmlComment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml/tsxmlDeclaration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml/tsxmlDocument.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml/tsxmlElement.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml/tsxmlJSONConverter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml/tsxmlModelDocument.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml/tsxmlNode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml/tsxmlPatchDocument.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml/tsxmlRunningDocument.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml/tsxmlText.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml/tsxmlTweaks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml/tsxmlUnknown.cpp
)

if(UNIX)
    list(APPEND tsduck_base_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/unix/tsSysCtl.cpp)
endif()

if(LINUX)
    list(APPEND tsduck_base_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/linux/tsSignalAllocator.cpp)
endif()
if(APPLE)
    list(APPEND tsduck_base_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/mac/tsMacPList.cpp)
endif()
if(WIN32)
    list(APPEND tsduck_base_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/windows/tsComIds.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/windows/tsWinUtils.cpp
    )
endif()
if(WIN32)
    list(APPEND tsduck_base_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/windows/tsWebRequestGuts.cpp)
elseif(UNIX)
    list(APPEND tsduck_base_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/unix/tsWebRequestGuts.cpp)
endif()


set(tsduck_PUBLIC_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/algo
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/json
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/report
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/system
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/text
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/types
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/xml
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dektec
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/analysis
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/cas
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/charset
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/aom
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/australia
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/avs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dtg
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/eacem
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/eutelsat
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/nordig
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/scte
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/sky
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/uwa
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/encap
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/epg
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/hls
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/mpe
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/pes
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/standards
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/t2mi
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/atsc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/isdb
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/mpeg
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/scte
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/teletext
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/timing
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/infra
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/vatek)

set(tsduck_PRIVATE_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/private
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/private
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dektec/private
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/private
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/private
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private)

if(WIN32)
    list(APPEND tsduck_PUBLIC_INCLUDES 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/windows
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/windows
    )
endif()
if(UNIX)
    list(APPEND tsduck_PUBLIC_INCLUDES 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/unix
    )
endif()
if(APPLE)
    list(APPEND tsduck_PUBLIC_INCLUDES 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/mac
    )
endif()
if(LINUX)
    list(APPEND tsduck_PUBLIC_INCLUDES 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/base/linux
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/linux
    )
endif()
if(BSD)
    list(APPEND tsduck_PUBLIC_INCLUDES 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/bsd
    )
endif()

# extra packages
find_package(PkgConfig)

if(WIN32)
    list(APPEND LIBTSDUCK_LINK_LIBRARIES ws2_32 bcrypt userenv winscard wininet quartz winmm ntdll ksuser strmiids wmcodecdspuuid)
endif(WIN32)

if(NODEPRECATE)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_DEPRECATE=1)
endif()

if(NOT NOPCSC)
    if(NOT WIN32) # winscard is available on Windows
        pkg_check_modules(PCSCLITE QUIET IMPORTED_TARGET pcsclite)
        if(NOT PCSCLITE_FOUND)
            set(NOPCSC ON)
        else()
            list(APPEND LIBTSDUCK_LINK_LIBRARIES PkgConfig::PCSCLITE)
        endif()
    endif()
endif()
if(NOPCSC)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_PCSC=1)
endif()

if(NOGITHUB)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_GITHUB=1)
endif()

if(NOHIDES)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_HIDES=1)
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
else()
    list(APPEND LIBTSDUCK_LINK_LIBRARIES PkgConfig::RIST)
endif()

set(SRT_FOUND 0)
if(NOT NOSRT)
    find_package(SRT QUIET)
endif()
if(NOT SRT_FOUND)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_SRT=1)
else()
    list(APPEND LIBTSDUCK_LINK_LIBRARIES SRT)
endif()

if(NOT WIN32)
    if(BUILD_SHARED_LIBS)
        find_package(OpenSSL REQUIRED)
    else()
        # avoid linking problems between static and dynamic libraries for now
        list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_OPENSSL=1)
    endif()
endif()

if(NOT NODTAPI)
    include(FetchContent REQUIRED)
    if(WIN32)
        set(DTAPI_URL    "https://www.dektec.com/products/SDK/DTAPI/Downloads/LinuxSDK_v2025.01.0.tar.gz")
        set(DTAPI_SHA512 "916388765ae2e8cf9416464ca4cd14c055858cea55a86ecb9e2bfc29ead193e871aa380b9648f795a37a13e328faff7dffd1016eacf8173f14b7311bd528fee6")
    elseif(LINUX)
        set(DTAPI_URL    "https://www.dektec.com/products/SDK/DTAPI/Downloads/WinSDK_v2025.01.0.zip")
        set(DTAPI_SHA512 "04d048ad9ed2098c38586d6695498e5ec3da9fa835d4e0602343f1effe21fdca0efbec1579b7a39421e49a12edc63f6c86a5dbf48cf1f2b8aba2df778e368302")
    endif()
    FetchContent_Declare(
        DTAPI
        DOWNLOAD_EXTRACT_TIMESTAMP ON
        URL      ${DTAPI_URL}
        URL_HASH SHA512=${DTAPI_SHA512}
    )
    FetchContent_MakeAvailable(DTAPI)
    list(APPEND tsduck_PUBLIC_INCLUDES ${FETCHCONTENT_BASE_DIR}/dtapi-src/DTAPI/Include)
else()
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_DTAPI=1)
endif()

set(tsduck_dtv_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/analysis/tsContinuityAnalyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/analysis/tsTSAnalyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/analysis/tsTSAnalyzerOptions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/analysis/tsTSAnalyzerReport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsBitrateDifferenceDVBT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsChannelFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsDeliverySystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsHFBand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsLegacyBandWidth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsLNB.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsModulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsModulationArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsSignalState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsTSScanner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsTuner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsTunerArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsTunerBase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/broadcast/tsTunerEmulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/cas/tsCASFamily.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/cas/tsCASMapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/cas/tsCASSelectionArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/cas/tsKeyTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/cas/tsPCSC.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/cas/tsPIDOperator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/cas/tsTSScrambling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/charset/tsARIBCharset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/charset/tsARIBCharsetData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/charset/tsARIBCharsetEncoding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/charset/tsCharset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/charset/tsDumpCharset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/charset/tsDVBCharset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/charset/tsDVBCharTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/charset/tsDVBCharTableSingleByte.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/charset/tsDVBCharTableUTF16.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/charset/tsDVBCharTableUTF8.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAbstractAudioVideoAttributes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAbstractAVCAccessUnit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAbstractHEVCAccessUnit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAbstractVideoAccessUnit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAbstractVideoData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAbstractVideoStructure.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAbstractVVCAccessUnit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAC3Attributes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAccessUnitIterator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAVCAccessUnitDelimiter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAVCAttributes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAVCHRDParameters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAVCParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAVCSequenceParameterSet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsAVCVUIParameters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsCodecType.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsHEVCAccessUnitDelimiter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsHEVCAttributes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsHEVCHRDParameters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsHEVCProfileTierLevel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsHEVCScalingListData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsHEVCSequenceParameterSet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsHEVCShortTermReferencePictureSetList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsHEVCVUIParameters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsMPEG2AudioAttributes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsMPEG2VideoAttributes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/codec/tsVVCAccessUnitDelimiter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsAbstractDemux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsAbstractPacketizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsCyclingPacketizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsDemuxedData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsFileNameRate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsFileNameRateList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsInvalidSectionHandlerInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsOneShotPacketizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsPacketizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsSectionDemux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsSectionHandlerInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsSectionProviderInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsSignalizationDemux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsSignalizationHandlerInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsStandaloneTableDemux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsTableHandlerInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/demux/tsTimeTrackerDemux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/aom/tsAV1VideoDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc/tsATSCAC3AudioStreamDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc/tsATSCEAC3AudioDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc/tsATSCStuffingDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc/tsATSCTimeShiftedServiceDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc/tsCaptionServiceDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc/tsComponentNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc/tsContentAdvisoryDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc/tsDCCArrivingRequestDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc/tsDCCDepartingRequestDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc/tsExtendedChannelNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc/tsGenreDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc/tsRedistributionControlDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/atsc/tsServiceLocationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/australia/tsAustraliaLogicalChannelDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/avs/tsAVS3VideoDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dtg/tsDTGGuidanceDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dtg/tsDTGHDSimulcastLogicalChannelDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dtg/tsDTGLogicalChannelDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dtg/tsDTGPreferredNameIdentifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dtg/tsDTGPreferredNameListDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dtg/tsDTGServiceAttributeDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dtg/tsDTGShortServiceNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsAACDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsAbstractDeliverySystemDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsAbstractLogicalChannelDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsAbstractMultilingualDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsAbstractPreferredNameIdentifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsAbstractPreferredNameListDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsAdaptationFieldDataDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsAncillaryDataDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsAnnouncementSupportDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsApplicationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsApplicationIconsDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsApplicationNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsApplicationRecordingDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsApplicationSignallingDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsApplicationStorageDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsApplicationUsageDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsAudioPreselectionDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsBouquetNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsC2BundleDeliverySystemDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsC2DeliverySystemDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsCableDeliverySystemDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsCAIdentifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsCellFrequencyLinkDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsCellListDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsCIAncillaryDataDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsComponentDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsContentDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsContentIdentifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsCountryAvailabilityDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsCPCMDeliverySignallingDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsCPDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsCPIdentifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDataBroadcastDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDataBroadcastIdDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDefaultAuthorityDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDIILocationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDSNGDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDTSDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDTSHDDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDTSNeuralDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBAC3Descriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBAC4Descriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBDTSUHDDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBEnhancedAC3Descriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBHTMLApplicationBoundaryDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBHTMLApplicationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBHTMLApplicationLocationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBJApplicationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBJApplicationLocationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBServiceProminenceDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBStuffingDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsDVBTimeShiftedServiceDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsECMRepetitionRateDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsExtendedEventDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsExternalApplicationAuthorizationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsFrequencyListDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsFTAContentManagementDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsGraphicsConstraintsDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsImageIconDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsIPMACGenericStreamLocationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsIPMACPlatformNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsIPMACPlatformProviderNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsIPMACStreamLocationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsIPSignallingDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsISPAccessModeDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsLinkageDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsLocalTimeOffsetDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsMessageDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsMosaicDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsMultilingualBouquetNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsMultilingualComponentDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsMultilingualNetworkNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsMultilingualServiceNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsNetworkChangeNotifyDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsNetworkNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsNVODReferenceDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsParentalRatingDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsPartialTransportStreamDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsPDCDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsPrefetchDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsPrivateDataSpecifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsProtectionMessageDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsRARoverDVBstreamDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsRARoverIPDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsRelatedContentDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsRNTScanDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsS2SatelliteDeliverySystemDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsS2XSatelliteDeliverySystemDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsS2Xv2SatelliteDeliverySystemDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSatelliteDeliverySystemDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSchedulingDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsScramblingDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsServiceAvailabilityDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsServiceDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsServiceIdentifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsServiceListDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsServiceMoveDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsServiceRelocatedDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSHDeliverySystemDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsShortEventDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsShortSmoothingBufferDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSimpleApplicationBoundaryDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSimpleApplicationLocationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSSUDataBroadcastIdDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSSUEnhancedMessageDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSSUEventNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSSULinkageDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSSULocationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSSUMessageDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSSUSubgroupAssociationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSSUURIDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsStreamIdentifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSubtitlingDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsSupplementaryAudioDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsT2DeliverySystemDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsT2MIDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTargetIPAddressDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTargetIPSlashDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTargetIPSourceSlashDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTargetIPv6AddressDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTargetIPv6SlashDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTargetIPv6SourceSlashDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTargetMACAddressDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTargetMACAddressRangeDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTargetRegionDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTargetRegionNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTargetSerialNumberDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTargetSmartcardDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTelephoneDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTeletextDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTerrestrialDeliverySystemDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTimeShiftedEventDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTimeSliceFECIdentifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTransportProtocolDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTransportStreamDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTTMLSubtitlingDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsTVAIdDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsUpdateDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsURILinkageDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsVBIDataDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsVBITeletextDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsVideoDepthRangeDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/dvb/tsVVCSubpicturesDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/eacem/tsEacemHDSimulcastLogicalChannelDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/eacem/tsEacemLogicalChannelNumberDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/eacem/tsEacemPreferredNameIdentifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/eacem/tsEacemPreferredNameListDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/eacem/tsEacemStreamIdentifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/eutelsat/tsEutelsatChannelNumberDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsAbstractDownloadContentDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsAreaBroadcastingInformationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsAudioComponentDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsBasicLocalEventDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsBoardInformationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsBroadcasterNameDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsCAContractInfoDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsCAEMMTSDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsCarouselCompatibleCompositeDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsCAServiceDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsConditionalPlaybackDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsContentAvailabilityDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsDataComponentDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsDataContentDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsDigitalCopyControlDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsDownloadContentDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsEmergencyInformationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsEventGroupDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsExtendedBroadcasterDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsHierarchicalTransmissionDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsHybridInformationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsISDBAccessControlDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsISDBComponentGroupDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsISDBConnectedTransmissionDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsISDBHyperlinkDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsISDBLDTLinkageDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsISDBNetworkIdentifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsISDBTargetRegionDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsISDBTerrestrialDeliverySystemDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsLogoTransmissionDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsNetworkDownloadContentDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsNodeRelationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsPartialReceptionDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsPartialTSTimeDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsReferenceDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsSeriesDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsServiceGroupDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsShortNodeInformationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsSIParameterDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsSIPrimeTSDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsSTCReferenceDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsSystemManagementDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsTSInformationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/isdb/tsVideoDecodeControlDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsAFExtensionsDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsAssociationTagDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsAudioStreamDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsAuxiliaryVideoStreamDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsAVCTimingAndHRDDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsAVCVideoDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsCADescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsCarouselIdentifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsContentLabellingDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsCopyrightDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsDataStreamAlignmentDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsDeferredAssociationTagsDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsEVCTimingAndHRDDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsEVCVideoDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsExternalESIdDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsFMCDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsGreenExtensionDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsHEVCHierarchyExtensionDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsHEVCOperationPointDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsHEVCSubregionDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsHEVCTileSubstreamDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsHEVCTimingAndHRDDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsHEVCVideoDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsHierarchyDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsIBPDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsISO639LanguageDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsJ2KVideoDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsJPEGXSVideoDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsLCEVCLinkageDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsLCEVCVideoDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsM4MuxBufferSizeDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsM4MuxTimingDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMasteringDisplayMetadata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMaximumBitrateDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMediaServiceKindDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMetadataDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMetadataPointerDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMetadataSTDDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMPEG2AACAudioDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMPEG2StereoscopicVideoFormatDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMPEG4AudioDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMPEG4TextDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMPEG4VideoDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMPEGH3DAudioDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMPEGH3DAudioMultiStreamDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMPEGH3DAudioSceneDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMPEGH3DAudioTextLabelDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMultiplexBufferDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMultiplexBufferUtilizationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMuxcodeDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMVCExtensionDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsMVCOperationPointDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsNPTEndpointDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsNPTReferenceDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsPrivateDataIndicatorDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsQualityExtensionDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsRegistrationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsSLDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsSmoothingBufferDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsSTDDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsStereoscopicProgramInfoDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsStereoscopicVideoInfoDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsStreamEventDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsStreamModeDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsSVCExtensionDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsSystemClockDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsTargetBackgroundGridDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsTransportProfileDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsVideoStreamDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsVideoWindowDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsVirtualSegmentationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsVVCTimingAndHRDDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/mpeg/tsVVCVideoDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/nordig/tsNorDigLogicalChannelDescriptorV1.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/nordig/tsNorDigLogicalChannelDescriptorV2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/scte/tsCueIdentifierDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/scte/tsEASAudioFileDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/scte/tsEASInbandDetailsChannelDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/scte/tsEASInbandExceptionChannelsDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/scte/tsEASMetadataDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/scte/tsSpliceAvailDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/scte/tsSpliceDTMFDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/scte/tsSpliceSegmentationDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/scte/tsSpliceTimeDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/sky/tsSkyLogicalChannelNumberDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/tsAbstractDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/descriptors/uwa/tsUWAVideoStreamDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tsDuckProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tsECMGClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tsECMGClientArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tsECMGClientHandlerInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tsECMGSCS.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tsEMMGClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tsEMMGMUX.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tsSimulCryptDate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tstlvAnalyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tstlvChannelMessage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tstlvLogger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tstlvMessage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tstlvMessageFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tstlvProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tstlvSerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/dvbsimulcrypt/tstlvStreamMessage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/encap/tsPacketDecapsulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/encap/tsPacketEncapsulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/epg/tsEITGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/epg/tsEITProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/epg/tsEITRepetitionProfile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/hls/tshls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/hls/tshlsAltPlayList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/hls/tshlsMediaElement.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/hls/tshlsMediaPlayList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/hls/tshlsMediaSegment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/hls/tshlsPlayList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/hls/tshlsTagAttributes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/mpe/tsMPEDemux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/mpe/tsMPEHandlerInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/mpe/tsMPEPacket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/pes/tsPES.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/pes/tsPESDemux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/pes/tsPESHandlerInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/pes/tsPESOneShotPacketizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/pes/tsPESPacket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/pes/tsPESPacketizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/pes/tsPESProviderInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/pes/tsPESStreamPacketizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsAbstractDefinedByStandards.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsAbstractSignalization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsATSCMultipleString.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsAudioLanguageOptions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsBinaryTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsDescriptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsDescriptorList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsLogicalChannelNumbers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsMJD.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsPSI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsPSIBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsPSILogger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsPSIMerger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsPSIRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsSCTE35.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsSection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsSectionFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsSectionFileArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsServiceDiscovery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsServiceIdTriplet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsSpliceInsert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsSpliceSchedule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsTablePatchXML.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsTablesDisplay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsTablesLogger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsTablesLoggerFilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsTablesLoggerFilterInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsTablesLoggerFilterRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/signalization/tsTransportStreamId.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/standards/tsDuckContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/standards/tsNames.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/standards/tsStandards.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/t2mi/tsT2MIDemux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/t2mi/tsT2MIHandlerInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/t2mi/tsT2MIPacket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/atsc/tsATSCEIT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/atsc/tsCVCT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/atsc/tsDCCSCT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/atsc/tsDCCT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/atsc/tsETT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/atsc/tsMGT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/atsc/tsRRT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/atsc/tsSTT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/atsc/tsTVCT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/atsc/tsVCT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsAIT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsBAT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsCIT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsDiscontinuityInformationTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsEIT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsINT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsNIT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsRCT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsRNT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsRST.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsSAT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsSDT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsSelectionInformationTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsTDT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsTOT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/dvb/tsUNT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/isdb/tsBIT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/isdb/tsCDT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/isdb/tsERT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/isdb/tsITT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/isdb/tsLDT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/isdb/tsLIT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/isdb/tsNBIT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/isdb/tsPCAT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/isdb/tsSDTT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/mpeg/tsCAT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/mpeg/tsDSMCCStreamDescriptorsTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/mpeg/tsPAT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/mpeg/tsPMT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/mpeg/tsTSDT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/scte/tsCableEmergencyAlertTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/scte/tsSpliceInformationTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/tsAbstractDescriptorsTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/tsAbstractLongTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/tsAbstractTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/tables/tsAbstractTransportListTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/teletext/tsSubRipGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/teletext/tsTeletextCharset.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/teletext/tsTeletextDemux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/teletext/tsTeletextHandlerInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/timing/tsBitRateRegulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/timing/tsPacketInsertionController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/timing/tsPCRAnalyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/timing/tsPCRMerger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/timing/tsPCRRegulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/timing/tsTimeShiftBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/timing/tsTSSpeedMetrics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTimeSource.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTS.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSDatagramOutput.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSDatagramOutputHandlerInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSDumpArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSFile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSFileInputArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSFileInputBuffered.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSFileOutputArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSFileOutputResync.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSForkPipe.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSFuzzing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSFuzzingArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSPacket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSPacketFormat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSPacketMetadata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSPacketQueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSPacketStream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/transport/tsTSPacketWindow.cpp
)

if(WIN32)
    list(APPEND tsduck_dtv_SOURCES 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/windows/tsDirectShowFilterCategory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/windows/tsDirectShowGraph.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/windows/tsDirectShowNetworkType.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/windows/tsDirectShowTest.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/windows/tsDirectShowUtils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/windows/tsMediaTypeUtils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/windows/tsSinkFilter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/windows/tsTunerDevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/windows/tsTunerGraph.cpp
    )
endif()
if(LINUX)
    list(APPEND tsduck_dtv_SOURCES 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/linux/tsTunerDevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/linux/tsTunerDeviceInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/linux/tsDTVProperties.cpp
    )
endif()
if(APPLE)
    list(APPEND tsduck_dtv_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/mac/tsTunerDevice.cpp)
endif()
if(BSD)
    list(APPEND tsduck_dtv_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dtv/bsd/tsTunerDevice.cpp)
endif()

set(tsduck_crypto_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/private/tsCRC32.accel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/private/tsInitCryptoLibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsAES128.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsAES256.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsBetterSystemRandomGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsBlockCipher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsBlockCipherAlertInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsBlockCipherProperties.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsCRC32.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsCryptoLibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsDES.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsDVBCISSA.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsDVBCSA2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsHash.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsIDSA.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsRandomGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsSCTE52.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsSHA1.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsSHA256.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsSHA512.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsSystemRandomGenerator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsTDES.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/crypto/tsXoshiro256ss.cpp
)

set(tsduck_plugins_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/apps/tsInputSwitcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/apps/tsInputSwitcherArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/apps/tsLatencyMonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/apps/tsLatencyMonitorArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/apps/tsMuxer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/apps/tsMuxerArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/apps/tsTSPControlCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/apps/tsTSProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/apps/tsTSProcessorArgs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/infra/tsArgsWithPlugins.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/infra/tsDuckExtensionRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/infra/tsPluginEventContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/infra/tsPluginEventData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/infra/tsPluginEventHandlerInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/infra/tsPluginEventHandlerRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/infra/tsPluginOptions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/infra/tsPluginRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/infra/tsPluginThread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/infra/tsTSP.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tsRISTPluginData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstslatencymonitorInputExecutor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstsmuxCore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstsmuxInputExecutor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstsmuxOutputExecutor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstsmuxPluginExecutor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstspControlServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstspInputExecutor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstspJointTermination.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstspOutputExecutor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstspPluginExecutor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstspProcessorExecutor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstsswitchCommandListener.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstsswitchCore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstsswitchEventDispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstsswitchInputExecutor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstsswitchOutputExecutor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/private/tstsswitchPluginExecutor.cpp
)

set(tsduck_plugins_plugins_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsAbstractDatagramInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsAbstractDescrambler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsAbstractDuplicateRemapPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsAbstractHTTPInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsAbstractTablePlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsDebugPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsDropOutputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsFileInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsFileOutputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsFilePacketPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsForkInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsForkOutputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsForkPacketPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tshlsInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tshlsOutputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsHTTPInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsHTTPOutputPlugin.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsIPInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsIPOutputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsIPPacketPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsMemoryInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsMemoryOutputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsNullInputPlugin.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsOutputPlugin.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsPlugin.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsProcessorPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsPSIPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsPushInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsRISTInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsRISTOutputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsSkipPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsSRTInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsSRTOutputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsTablesPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsTeletextPlugin.cpp
)

set(tsduck_plugins_static_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsOutputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/plugins/plugins/tsProcessorPlugin.cpp
)

set(tsduck_vatek_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/vatek/tsVatekControl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/vatek/tsVatekOutputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/vatek/tsVatekUtils.cpp
)
if(NOT NOVATEK)
    find_package(vatek QUIET)
endif()
if(NOT NOVATEK_FOUND)
    list(APPEND LIBTSDUCK_CXXFLAGS_INCLUDES TS_NO_VATEK=1)
else()
    list(APPEND LIBTSDUCK_LINK_LIBRARIES vatek)
endif()

set(tsduck_dektec_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dektec/private/tsDektecArgsUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dektec/private/tsDektecDevice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dektec/private/tsDektecVPD.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dektec/private/tsDVBT2ParamsEvaluator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dektec/tsDektecControl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dektec/tsDektecDeviceInfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dektec/tsDektecInputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dektec/tsDektecOutputPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/dektec/tsDektecUtils.cpp
)

set(tsduck_hides_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/tsHiDesDeviceInfo.cpp)
if(WIN32)
    list(APPEND tsduck_hides_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/windows/tsHiDesDevice.cpp)
elseif(APPLE)
    list(APPEND tsduck_hides_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/mac/tsHiDesDevice.cpp)
elseif(LINUX)
    list(APPEND tsduck_hides_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/linux/tsHiDesDevice.cpp)
elseif(BSD)
    list(APPEND tsduck_hides_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/libtsduck/hides/bsd/tsHiDesDevice.cpp)
endif()

list(APPEND TSPLUGINS 
    aes analyze bat bitrate_monitor boostpid cat clear continuity count craft
    cutoff datainject decap descrambler dump duplicate dvb eit eitinject encap
    feed filter fuzz history inject limit merge mpe mpeinject mux nit
    nitscan pat pattern pcap pcradjust pcrbitrate pcrcopy pcredit pcrextract pcrverify
    pes pidshift play pmt psimerge reduce regulate remap rmorphan rmsplice scrambler
    sdt sections sifilter slice spliceinject splicemonitor stats stuffanalyze
    svremove svrename svresync t2mi time timeref timeshift trigger tsrename until zap)

if(NOT NOHIDES)
    list(APPEND TSPLUGINS hides)
endif()

if(BUILD_SHARED_LIBS)
    add_library(tsduck SHARED ${tsduck_base_SOURCES} ${tsduck_dtv_SOURCES} ${tsduck_crypto_SOURCES} ${tsduck_vatek_SOURCES} ${tsduck_dektec_SOURCES} ${tsduck_hides_SOURCES} ${tsduck_plugins_SOURCES} ${tsduck_plugins_static_SOURCES} ${tsduck_plugins_plugins_SOURCES})
    target_compile_definitions(tsduck
        PUBLIC ${BITRATE_FORMAT_FLAG}
        PUBLIC ${LIBTSDUCK_CXXFLAGS_INCLUDES}
        PRIVATE TS_WINVER_FILETYPE=VFT_DLL
        PRIVATE _TSDUCKDLL_IMPL
        PUBLIC _TSDUCKDLL_USE)
    target_include_directories(tsduck 
        PUBLIC ${tsduck_PUBLIC_INCLUDES}
        PRIVATE ${tsduck_PRIVATE_INCLUDES})
    target_include_directories(tsduck 
        PUBLIC $<BUILD_INTERFACE:${TSDUCK_H_DIRECTORY}>
        PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    set(tsduck_libs tsduck)

    target_link_libraries(tsduck PUBLIC ${LIBTSDUCK_LINK_LIBRARIES})
    install(TARGETS tsduck
        COMPONENT tsduck
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif(BUILD_SHARED_LIBS)

if(NOT BUILD_SHARED_LIBS)
    list(TRANSFORM TSPLUGINS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/src/tsplugins/tsplugin_ OUTPUT_VARIABLE tsplugin_static_SOURCES)
    list(TRANSFORM tsplugin_static_SOURCES APPEND ".cpp")

    add_library(tsducklib STATIC ${tsduck_base_SOURCES} ${tsduck_dtv_SOURCES} ${tsduck_crypto_SOURCES} ${tsduck_vatek_SOURCES} ${tsduck_dektec_SOURCES} ${tsduck_hides_SOURCES} ${tsduck_plugins_SOURCES} ${tsduck_plugins_static_SOURCES} ${tsduck_plugins_plugins_SOURCES} ${tsplugin_static_SOURCES})
    target_compile_definitions(tsducklib
        PUBLIC ${BITRATE_FORMAT_FLAG}
        PUBLIC TSDUCK_STATIC
        PUBLIC ${LIBTSDUCK_CXXFLAGS_INCLUDES}
        PRIVATE TSDUCK_STATIC_LIBRARY)
    target_include_directories(tsducklib 
        PUBLIC ${tsduck_PUBLIC_INCLUDES}
        PRIVATE ${tsduck_PRIVATE_INCLUDES})
    target_include_directories(tsducklib 
        PUBLIC $<BUILD_INTERFACE:${TSDUCK_H_DIRECTORY}>
        PUBLIC $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

    target_link_libraries(tsducklib PUBLIC ${CMAKE_DL_LIBS})
    set(tsduck_libs "$<LINK_LIBRARY:WHOLE_ARCHIVE,tsducklib>")

    target_link_libraries(tsducklib PRIVATE ${LIBTSDUCK_LINK_LIBRARIES})
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
