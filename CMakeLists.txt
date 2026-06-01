cmake_minimum_required(VERSION 3.20)
project(gateway_nema_dds CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# Pipeline: hilo POSIX read→parse→map→publish (D9/D13).
add_library(nmea_pipeline src/pipeline/Pipeline.cpp)
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
    fastdds)
target_compile_options(gateway_app PRIVATE -Wall -Wextra)

enable_testing()
add_subdirectory(tests)
