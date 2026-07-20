cmake_minimum_required(VERSION 3.20)
project(gateway_nema_dds CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Las libs núcleo son estáticas por diseño. find_package(rclcpp)/ament pone
# BUILD_SHARED_LIBS=ON en cache; lo fijamos OFF (variable normal, sombrea la cache)
# para que nmea_parser/registry/pipeline/mapper sigan siendo .a (sin -fPIC).
set(BUILD_SHARED_LIBS OFF)

# Núcleo del parser (hot path, freestanding): sin excepciones para hacer cumplir
# la disciplina sin-asignación / determinista (ver CONTEXT.md y docs/adr).
add_library(nmea_parser src/parser/Parser.cpp)
target_include_directories(nmea_parser PUBLIC src)
target_compile_options(nmea_parser PRIVATE -fno-exceptions -Wall -Wextra)

# Registro de sentencias: cold path, C++ normal con heap.
add_library(nmea_registry src/registry/Registry.cpp)
target_include_directories(nmea_registry PUBLIC src)
target_compile_options(nmea_registry PRIVATE -Wall -Wextra)

# Capa de captura: ISource / SerialSource / TcpSource / BaudDetector (D10/D13).
add_library(nmea_capture
    src/capture/SerialSource.cpp
    src/capture/TcpSource.cpp
    src/capture/UdpSource.cpp
    src/capture/BaudDetector.cpp)
target_include_directories(nmea_capture PUBLIC src)
target_link_libraries(nmea_capture PUBLIC nmea_parser)
target_compile_options(nmea_capture PRIVATE -Wall -Wextra)

# Fast DDS (requerido por mapper y pipeline).
find_package(fastdds 3 REQUIRED HINTS /opt/ros/lyrical/share/fastdds/cmake)

# ── Publicación ROS (Imu/NavSatFix): capa de app, fuera del núcleo ────────────
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

add_library(nmea_ros src/ros/RosPublisher.cpp)
target_include_directories(nmea_ros PUBLIC src)
target_link_libraries(nmea_ros PUBLIC rclcpp::rclcpp ${sensor_msgs_TARGETS})
target_compile_options(nmea_ros PRIVATE -Wall -Wextra)

# Pipeline: hilo POSIX read→parse→map→publish (D9/D13).
add_library(nmea_pipeline src/pipeline/Pipeline.cpp src/pipeline/PublishPlan.cpp)
target_include_directories(nmea_pipeline PUBLIC src)
target_link_libraries(nmea_pipeline PUBLIC nmea_capture nmea_mapper fastdds)
target_compile_options(nmea_pipeline PRIVATE -Wall -Wextra)

# Mapper SentenceView → DynamicData (XTypes, D5/D6).

add_library(nmea_mapper src/mapper/Mapper.cpp)
target_include_directories(nmea_mapper PUBLIC src)
target_link_libraries(nmea_mapper PUBLIC nmea_parser nmea_registry fastdds)
target_compile_options(nmea_mapper PRIVATE -Wall -Wextra)

# ── UI Qt: aplicación principal ──────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort Network)
set(CMAKE_AUTOMOC ON)  # genera moc automáticamente para Q_OBJECT

add_executable(gateway_app
    main.cpp
    src/ui/GatewayController.cpp
    src/ui/MainWindow.cpp
    src/ui/panels/InterfacesPanel.cpp
    src/ui/panels/DevicesPanel.cpp
    src/ui/panels/IdlPreviewPanel.cpp
    src/ui/panels/QoSPanel.cpp
    src/ui/panels/ConversionsPanel.cpp
    src/ui/panels/DdsMonitorPanel.cpp
    src/ui/models/ConversionModel.cpp
    src/ui/models/DdsTopicModel.cpp
    src/ui/QoSRecommender.cpp
)
target_include_directories(gateway_app PRIVATE src)
target_link_libraries(gateway_app PRIVATE
    nmea_pipeline nmea_mapper nmea_registry nmea_capture
    Qt6::Widgets Qt6::SerialPort Qt6::Network
    fastdds nmea_ros)
target_compile_options(gateway_app PRIVATE -Wall -Wextra)

enable_testing()
add_subdirectory(tests)

add_executable(verify_live tools/verify_live.cpp)
target_include_directories(verify_live PRIVATE src)
target_link_libraries(verify_live PRIVATE nmea_pipeline nmea_mapper nmea_registry nmea_capture fastdds)

# Arnés headless: lógica multi-interfaz de GatewayController contra UDP en vivo.
add_executable(test_multi_iface tools/test_multi_iface.cpp src/ui/GatewayController.cpp)
target_include_directories(test_multi_iface PRIVATE src)
target_link_libraries(test_multi_iface PRIVATE
    nmea_pipeline nmea_mapper nmea_registry nmea_capture Qt6::Core fastdds nmea_ros)

# Arnés headless: lectura serial real por la ruta de producción.
add_executable(test_serial tools/test_serial.cpp src/ui/GatewayController.cpp)
target_include_directories(test_serial PRIVATE src)
target_link_libraries(test_serial PRIVATE
    nmea_pipeline nmea_mapper nmea_registry nmea_capture Qt6::Core fastdds nmea_ros)

# Arnés headless: edición de QoS en vivo (recreación de writer).
add_executable(test_qos tools/test_qos.cpp src/ui/GatewayController.cpp)
target_include_directories(test_qos PRIVATE src)
target_link_libraries(test_qos PRIVATE
    nmea_pipeline nmea_mapper nmea_registry nmea_capture Qt6::Core fastdds nmea_ros)

# Validación interop: publicador de larga duración (datos reales del gateway).
add_executable(pub_hold tools/pub_hold.cpp src/ui/GatewayController.cpp)
target_include_directories(pub_hold PRIVATE src)
target_link_libraries(pub_hold PRIVATE
    nmea_pipeline nmea_mapper nmea_registry nmea_capture Qt6::Core fastdds nmea_ros)

# Arnés headless: validación GPS→ROS (NavSatFix) por la ruta de producción.
add_executable(verify_gps_ros tools/verify_gps_ros.cpp)
target_include_directories(verify_gps_ros PRIVATE src)
target_link_libraries(verify_gps_ros PRIVATE
    nmea_pipeline nmea_mapper nmea_registry nmea_capture fastdds nmea_ros)

# Arnés headless: ensayo de Demo 3 (QoS en caliente) y Demo 5 p.1 (reconvertir + ROS).
add_executable(rehearse_demos tools/rehearse_demos.cpp src/ui/GatewayController.cpp)
target_include_directories(rehearse_demos PRIVATE src)
target_link_libraries(rehearse_demos PRIVATE
    nmea_pipeline nmea_mapper nmea_registry nmea_capture Qt6::Core fastdds nmea_ros)

# Arnés offscreen: indicador de recepción/sin-datos en la capa de widgets (②/⑤).
add_executable(verify_stale_ui tools/verify_stale_ui.cpp
    src/ui/GatewayController.cpp
    src/ui/panels/DevicesPanel.cpp
    src/ui/panels/ConversionsPanel.cpp
    src/ui/models/ConversionModel.cpp)
target_include_directories(verify_stale_ui PRIVATE src)
target_link_libraries(verify_stale_ui PRIVATE
    nmea_pipeline nmea_mapper nmea_registry nmea_capture
    Qt6::Widgets Qt6::Network fastdds nmea_ros)

# Validación interop: suscriptor DDS independiente (sin IDL del gateway).
add_executable(dds_dynamic_sub tools/dds_dynamic_sub.cpp)
target_link_libraries(dds_dynamic_sub PRIVATE fastdds fastcdr)
