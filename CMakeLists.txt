cmake_minimum_required(VERSION 3.23)
project(dcc-proxy VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(CTest)
include(GNUInstallDirs)

# V25 control center plugin: installed as plugins_v1.1/<PLUGIN_NAME>.
set(PLUGIN_NAME dccproxy)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network DBus Qml Quick LinguistTools)
find_package(DdeControlCenter REQUIRED)

# Bundled YAML parser; no new runtime dependencies.
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/yaml-cpp EXCLUDE_FROM_ALL)

add_library(party-common STATIC
  src/api.cpp src/api.h
  src/config.cpp src/config.h
  src/geodata.cpp src/geodata.h
  src/nodeflags.cpp src/nodeflags.h
  src/subscriptionnodes.cpp src/subscriptionnodes.h
  src/tun.cpp src/tun.h
  src/proxybypass.cpp)
target_include_directories(party-common PUBLIC src)
target_link_libraries(party-common PUBLIC Qt6::Core Qt6::Network Qt6::DBus yaml-cpp)
target_compile_options(party-common PRIVATE -Wall -Wextra -Werror)

add_executable(dde-mihomo-daemon src/service.cpp)
target_link_libraries(dde-mihomo-daemon PRIVATE party-common Qt6::Qml)
target_compile_options(dde-mihomo-daemon PRIVATE -Wall -Wextra -Werror)

# Plugin data object (dccData) plus its controllers.
add_library(${PLUGIN_NAME} MODULE
  src/plugin.cpp
  src/plugindata.h
  src/servicecontroller.cpp
  src/proxycontroller.cpp
  src/profilecontroller.cpp
  src/datacontrollers.cpp
  src/settingscontroller.cpp
  src/resources.qrc)
target_link_libraries(${PLUGIN_NAME} PRIVATE party-common Dde::Control-Center Qt6::Core Qt6::Network Qt6::Qml)
target_compile_options(${PLUGIN_NAME} PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(${PLUGIN_NAME} PRIVATE PARTY_VERSION="${PROJECT_VERSION}")
# A MODULE loaded by the control center must not rely on the host for our own
# symbols; without this a missing moc silently produces a plugin that fails to
# load with "undefined symbol".
target_link_options(${PLUGIN_NAME} PRIVATE -Wl,--no-undefined)

file(GLOB PLUGIN_QML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml" "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.js")
dcc_install_plugin(NAME ${PLUGIN_NAME} TARGET ${PLUGIN_NAME}
  QML_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/qml"
  QML_FILES ${PLUGIN_QML_FILES})
dcc_handle_plugin_translation(NAME ${PLUGIN_NAME} SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src"
  QML_FILES ${PLUGIN_QML_FILES}
  SOURCE_FILES src/plugin.cpp src/servicecontroller.cpp src/proxycontroller.cpp src/profilecontroller.cpp
    src/datacontrollers.cpp src/settingscontroller.cpp src/api.cpp src/config.cpp src/tun.cpp src/proxybypass.cpp)

install(TARGETS dde-mihomo-daemon RUNTIME DESTINATION lib/dcc-proxy)
install(FILES packaging/dde-mihomo-daemon.service DESTINATION lib/systemd/user)
install(FILES README.md DESTINATION share/doc/dcc-proxy)
install(DIRECTORY docs DESTINATION share/doc/dcc-proxy)
install(FILES LICENSE DESTINATION share/doc/dcc-proxy)
install(FILES third_party/yaml-cpp/LICENSE DESTINATION share/doc/dcc-proxy RENAME yaml-cpp.LICENSE)
install(FILES third_party/flag-icons/LICENSE DESTINATION share/doc/dcc-proxy RENAME flag-icons.LICENSE)

set(MIHOMO_BINARY "" CACHE FILEPATH "Optional native Mihomo executable to bundle")
set(MIHOMO_SOURCE_ARCHIVE "" CACHE FILEPATH "Corresponding source archive for bundled Mihomo")
if(MIHOMO_BINARY)
  if(NOT EXISTS "${MIHOMO_SOURCE_ARCHIVE}")
    message(FATAL_ERROR "Bundled core requires MIHOMO_SOURCE_ARCHIVE")
  endif()
  install(PROGRAMS "${MIHOMO_BINARY}" DESTINATION lib/dcc-proxy RENAME mihomo)
  install(FILES "${MIHOMO_SOURCE_ARCHIVE}" DESTINATION share/doc/dcc-proxy RENAME mihomo-source.tar.gz)
  install(FILES packaging/mihomo.LICENSE DESTINATION share/doc/dcc-proxy)
endif()
set(MIHOMO_GEODATA_DIR "" CACHE PATH "Directory containing verified official offline geodata")
if(MIHOMO_GEODATA_DIR)
  foreach(database Country.mmdb GeoIP.dat GeoSite.dat ASN.mmdb)
    if(NOT EXISTS "${MIHOMO_GEODATA_DIR}/${database}")
      message(FATAL_ERROR "Missing bundled ${database}; run tools/prepare-geodata.py")
    endif()
    install(FILES "${MIHOMO_GEODATA_DIR}/${database}" DESTINATION share/dcc-proxy)
  endforeach()
  install(FILES packaging/geodata.json DESTINATION share/dcc-proxy)
  install(FILES packaging/geodata.LICENSE DESTINATION share/doc/dcc-proxy)
  install(FILES packaging/geodata-NOTICES.md DESTINATION share/doc/dcc-proxy)
endif()

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_CONTACT "DCC Clash Party maintainers")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native Clash / Mihomo control center plugin for V25")
set(CPACK_DEBIAN_PACKAGE_SECTION net)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "dde-control-center (>= 6.1), libdtk6declarative, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-dialogs, qml6-module-qtquick-layouts, qml6-module-qtquick-window, libqt6svg6")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst")
include(CPack)

if(BUILD_TESTING)
  find_package(Qt6 REQUIRED COMPONENTS Test)
  add_executable(party-tests tests/test_plugindata.cpp src/plugindata.h src/plugin.cpp src/servicecontroller.cpp
    src/proxycontroller.cpp src/profilecontroller.cpp src/datacontrollers.cpp src/settingscontroller.cpp src/resources.qrc)
  add_dependencies(party-tests dde-mihomo-daemon)
  set_property(TARGET party-tests APPEND PROPERTY AUTOMOC_MACRO_NAMES "DCC_FACTORY_CLASS")
  target_link_libraries(party-tests PRIVATE party-common Dde::Control-Center Qt6::Test Qt6::Qml)
  add_test(NAME party-tests COMMAND party-tests)
  set_tests_properties(party-tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
