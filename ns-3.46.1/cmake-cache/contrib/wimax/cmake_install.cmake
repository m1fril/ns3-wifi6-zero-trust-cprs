# Install script for directory: /Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/build/lib/libns3.46.1-wimax-optimized.dylib")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.46.1-wimax-optimized.dylib" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.46.1-wimax-optimized.dylib")
    execute_process(COMMAND /usr/bin/install_name_tool
      -delete_rpath "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/build/lib"
      -add_rpath "/usr/local/lib:$ORIGIN/:$ORIGIN/../lib:/usr/local/lib64:$ORIGIN/:$ORIGIN/../lib64"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.46.1-wimax-optimized.dylib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" -x "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.46.1-wimax-optimized.dylib")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ns3" TYPE FILE FILES
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/helper/wimax-helper.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/wimax-channel.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/wimax-net-device.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/bs-net-device.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/ss-net-device.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/cid.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/cid-factory.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/ofdm-downlink-frame-prefix.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/wimax-connection.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/ss-record.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/mac-messages.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/dl-mac-messages.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/ul-mac-messages.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/wimax-phy.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/simple-ofdm-wimax-phy.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/simple-ofdm-wimax-channel.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/send-params.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/service-flow.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/ss-manager.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/connection-manager.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/wimax-mac-header.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/wimax-mac-queue.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/crc8.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/service-flow-manager.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/bs-uplink-scheduler.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/bs-uplink-scheduler-simple.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/bs-uplink-scheduler-mbqos.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/bs-uplink-scheduler-rtps.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/ul-job.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/bs-scheduler.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/bs-scheduler-simple.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/bs-scheduler-rtps.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/service-flow-record.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/snr-to-block-error-rate-record.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/snr-to-block-error-rate-manager.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/simple-ofdm-send-param.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/ss-service-flow-manager.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/bs-service-flow-manager.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/cs-parameters.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/ipcs-classifier-record.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/wimax-tlv.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/ipcs-classifier.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/bvec.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/contrib/wimax/model/wimax-mac-to-mac-header.h"
    "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/build/include/ns3/wimax-module.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/mykhail/Documents/diplom/ns-allinone-3.46.1/ns-3.46.1/cmake-cache/contrib/wimax/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
