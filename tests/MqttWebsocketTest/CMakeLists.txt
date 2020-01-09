# The following four lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# (Not part of the boilerplate)
# This WIFICON uses an extra component for common functions such as Wi-Fi and Ethernet connection.


set(SUPPORTED_TARGETS esp32)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS ../../src/components/wifi_connection)
project(mqtt_websocket_test)

target_add_binary_data(mqtt_websocket_test.elf "main/mqtt_eclipse_org.pem" TEXT)

if  (CONFIG_WIFICON_VALIDATE_SERVER_CERT)
	target_add_binary_data(mqtt_websocket_test.elf ./main/${CONFIG_WIFICON_VALIDATE_SERVER_CERT_NAME} TEXT)
endif()

if  (CONFIG_WIFICON_EAP_METHOD_TLS)
	target_add_binary_data(mqtt_websocket_test.elf ./main/${CONFIG_WIFICON_EAP_METHOD_TLS_CERT_NAME} TEXT)
	target_add_binary_data(mqtt_websocket_test.elf ./main/${CONFIG_WIFICON_EAP_METHOD_TLS_KEY_NAME} TEXT)
endif()

target_add_binary_data(mqtt_websocket_test.elf "main/ota/certs/test_cert.pem" TEXT)
