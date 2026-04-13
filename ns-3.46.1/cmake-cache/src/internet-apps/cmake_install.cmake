# Install script for directory: /Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/build/lib/libns3.46.1-internet-apps-optimized.dylib")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.46.1-internet-apps-optimized.dylib" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.46.1-internet-apps-optimized.dylib")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/build/lib"
      -add_rpath "/usr/local/lib:$ORIGIN/:$ORIGIN/../lib:/usr/local/lib64:$ORIGIN/:$ORIGIN/../lib64"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.46.1-internet-apps-optimized.dylib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" -x "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.46.1-internet-apps-optimized.dylib")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ns3" TYPE FILE FILES
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/helper/dhcp-helper.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/helper/dhcp6-helper.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/helper/ping-helper.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/helper/radvd-helper.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/helper/v4traceroute-helper.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/model/dhcp-client.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/model/dhcp-header.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/model/dhcp-server.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/model/dhcp6-client.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/model/dhcp6-duid.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/model/dhcp6-header.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/model/dhcp6-options.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/model/dhcp6-server.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/model/ping.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/model/radvd-interface.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/model/radvd-prefix.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/model/radvd.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/src/internet-apps/model/v4traceroute.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/build/include/ns3/internet-apps-module.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/cmake-cache/src/internet-apps/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
