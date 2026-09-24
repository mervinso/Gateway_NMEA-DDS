cmake_minimum_required(VERSION 3.20)
project(gateway_nema_dds CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sin CMAKE_BUILD_TYPE, GCC y Clang compilan sin optimizar (-O0). Cualquier
# medición de rendimiento hecha sobre esa construcción es inservible, y el
# descuido no deja rastro visible. Se fija Release por defecto y el tipo
# efectivo queda escrito en el manifiesto de construcción.
get_property(_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Tipo de construcción: Release | RelWithDebInfo | Debug | MinSizeRel" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Release RelWithDebInfo Debug MinSizeRel)
    message(STATUS "CMAKE_BUILD_TYPE no especificado: se usa Release.")
endif()

# Las libs núcleo son estáticas por diseño. find_package(rclcpp)/ament pone
# BUILD_SHARED_LIBS=ON en cache; lo fijamos OFF (variable normal, sombrea la cache)
# para que nmea_parser/registry/pipeline/mapper sigan siendo .a (sin -fPIC).
set(BUILD_SHARED_LIBS OFF)

# ─────────────────────────────────────────────────────────────────────────────
# Opciones de construcción
#
# Tres estados por componente opcional:
#   AUTO (por defecto) — se activa si sus dependencias están presentes, y se
#                        omite con un aviso si no lo están.
#   ON               — se exige; la configuración falla si faltan dependencias.
#   OFF              — se excluye aunque las dependencias existan.
#
# AUTO conserva el comportamiento habitual en la máquina de desarrollo (con el
# entorno de ROS 2 cargado y Qt instalado, todo se construye) y a la vez permite
# que un tercero clone y compile el núcleo sin ROS ni Qt. Para las corridas de
# medición debe usarse ON u OFF de forma explícita, nunca AUTO: el manifiesto de
# la corrida tiene que registrar una decisión, no un resultado de detección.
# ─────────────────────────────────────────────────────────────────────────────
set(GATEWAY_ROS_BRIDGE AUTO CACHE STRING
    "Puente ROS 2 (rclcpp / sensor_msgs): AUTO | ON | OFF")
set_property(CACHE GATEWAY_ROS_BRIDGE PROPERTY STRINGS AUTO ON OFF)

set(GATEWAY_BUILD_UI AUTO CACHE STRING
    "Interfaz de escritorio Qt 6 Widgets: AUTO | ON | OFF")
set_property(CACHE GATEWAY_BUILD_UI PROPERTY STRINGS AUTO ON OFF)

set(GATEWAY_BUILD_TESTS AUTO CACHE STRING
    "Pruebas con GoogleTest: AUTO | ON | OFF")
set_property(CACHE GATEWAY_BUILD_TESTS PROPERTY STRINGS AUTO ON OFF)

# No se llama FASTDDS_ROOT a propósito: CMake reserva <PAQUETE>_ROOT en
# mayúsculas y find_package emite una advertencia de política al verlo definido.
set(GATEWAY_FASTDDS_ROOT "" CACHE PATH
    "Prefijo de instalación de Fast DDS 3. Opcional: si se omite se busca en \
CMAKE_PREFIX_PATH y en el entorno.")

foreach(_opt GATEWAY_ROS_BRIDGE GATEWAY_BUILD_UI GATEWAY_BUILD_TESTS)
    string(TOUPPER "${${_opt}}" ${_opt})
    if(NOT ${_opt} MATCHES "^(AUTO|ON|OFF)$")
        message(FATAL_ERROR "${_opt} debe ser AUTO, ON u OFF; se recibió '${${_opt}}'.")
    endif()
endforeach()

# ─────────────────────────────────────────────────────────────────────────────
# Fast DDS — dependencia obligatoria del mapper y del pipeline.
#
# Antes se resolvía con HINTS /opt/ros/lyrical/share/fastdds/cmake, una ruta
# absoluta de una sola máquina. Se elimina: Fast DDS se localiza por el
# mecanismo estándar de CMake. Cuando el entorno de ROS 2 está cargado
# (`source /opt/ros/<distro>/setup.bash`) esa ruta ya entra por CMAKE_PREFIX_PATH,
# que es también como se encuentra rclcpp, de modo que el flujo de trabajo
# habitual no cambia. Quien no use ROS puede apuntar -DGATEWAY_FASTDDS_ROOT o
# -DCMAKE_PREFIX_PATH a su propia instalación.
# ─────────────────────────────────────────────────────────────────────────────
if(GATEWAY_FASTDDS_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${GATEWAY_FASTDDS_ROOT}")
endif()

find_package(fastdds 3 QUIET)
if(NOT fastdds_FOUND)
    message(FATAL_ERROR
        "No se encontró Fast DDS 3.\n"
        "  Indique su ubicación con una de estas opciones:\n"
        "    -DGATEWAY_FASTDDS_ROOT=/ruta/al/prefijo\n"
        "    -DCMAKE_PREFIX_PATH=/ruta/al/prefijo\n"
        "  Si Fast DDS proviene de una instalación de ROS 2, basta con cargar su\n"
        "  entorno antes de configurar:  source /opt/ros/<distro>/setup.bash")
endif()
message(STATUS "Fast DDS encontrado en: ${fastdds_DIR}")

# Directorios de bibliotecas en tiempo de ejecución, derivados del prefijo real
# de Fast DDS en vez de estar escritos a mano. Los usan las pruebas, que de otro
# modo no encuentran libfastdds.so cuando no está en la ruta del enlazador.
get_filename_component(GATEWAY_FASTDDS_PREFIX "${fastdds_DIR}/../../.." ABSOLUTE)
set(GATEWAY_RUNTIME_LIB_DIRS "${GATEWAY_FASTDDS_PREFIX}/lib")
if(CMAKE_LIBRARY_ARCHITECTURE)
    list(APPEND GATEWAY_RUNTIME_LIB_DIRS
         "${GATEWAY_FASTDDS_PREFIX}/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
endif()
list(JOIN GATEWAY_RUNTIME_LIB_DIRS ":" GATEWAY_RUNTIME_LD_PATH)

# ─────────────────────────────────────────────────────────────────────────────
# Resolución de los componentes opcionales
# ─────────────────────────────────────────────────────────────────────────────
set(GATEWAY_ROS_ENABLED OFF)
if(NOT GATEWAY_ROS_BRIDGE STREQUAL "OFF")
    if(GATEWAY_ROS_BRIDGE STREQUAL "ON")
        find_package(rclcpp REQUIRED)
        find_package(sensor_msgs REQUIRED)
        set(GATEWAY_ROS_ENABLED ON)
    else()
        find_package(rclcpp QUIET)
        find_package(sensor_msgs QUIET)
        if(rclcpp_FOUND AND sensor_msgs_FOUND)
            set(GATEWAY_ROS_ENABLED ON)
        else()
            message(STATUS
                "ROS 2 no encontrado: se omite el puente ROS y todo lo que depende de él "
                "(gateway_app, verify_gps_ros y los arneses de tools/). "
                "Para exigirlo use -DGATEWAY_ROS_BRIDGE=ON.")
        endif()
    endif()
endif()

set(GATEWAY_UI_ENABLED OFF)
if(NOT GATEWAY_BUILD_UI STREQUAL "OFF")
    if(GATEWAY_BUILD_UI STREQUAL "ON")
        find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort Network)
        set(GATEWAY_UI_ENABLED ON)
    else()
        find_package(Qt6 QUIET COMPONENTS Widgets SerialPort Network)
        if(Qt6_FOUND)
            set(GATEWAY_UI_ENABLED ON)
        else()
            message(STATUS
                "Qt 6 no encontrado: se omite la interfaz de escritorio. "
                "Para exigirla use -DGATEWAY_BUILD_UI=ON.")
        endif()
    endif()
endif()

if(GATEWAY_UI_ENABLED)
    set(CMAKE_AUTOMOC ON)  # genera moc automáticamente para Q_OBJECT
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Núcleo — sin Qt y sin ROS. Siempre se construye.
# ─────────────────────────────────────────────────────────────────────────────

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

# Mapper SentenceView → DynamicData (XTypes, D5/D6).
add_library(nmea_mapper src/mapper/Mapper.cpp)
target_include_directories(nmea_mapper PUBLIC src)
target_link_libraries(nmea_mapper PUBLIC nmea_parser nmea_registry fastdds)
target_compile_options(nmea_mapper PRIVATE -Wall -Wextra)

# Pipeline: hilo POSIX read→parse→map→publish (D9/D13).
add_library(nmea_pipeline src/pipeline/Pipeline.cpp src/pipeline/PublishPlan.cpp)
target_include_directories(nmea_pipeline PUBLIC src)
target_link_libraries(nmea_pipeline PUBLIC nmea_capture nmea_mapper fastdds)
target_compile_options(nmea_pipeline PRIVATE -Wall -Wextra)

# ─────────────────────────────────────────────────────────────────────────────
# Publicación ROS (Imu/NavSatFix): capa de app, fuera del núcleo.
# ─────────────────────────────────────────────────────────────────────────────
if(GATEWAY_ROS_ENABLED)
    add_library(nmea_ros src/ros/RosPublisher.cpp)
    target_include_directories(nmea_ros PUBLIC src)
    target_link_libraries(nmea_ros PUBLIC rclcpp::rclcpp ${sensor_msgs_TARGETS})
    target_compile_options(nmea_ros PRIVATE -Wall -Wextra)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Herramientas que no dependen ni de Qt ni de ROS. Son las que usa el banco de
# medición, y por eso se construyen siempre.
# ─────────────────────────────────────────────────────────────────────────────
add_executable(verify_live tools/verify_live.cpp)
target_include_directories(verify_live PRIVATE src)
target_link_libraries(verify_live PRIVATE
    nmea_pipeline nmea_mapper nmea_registry nmea_capture fastdds)

# Validación interop: suscriptor DDS independiente (sin IDL del gateway).
add_executable(dds_dynamic_sub tools/dds_dynamic_sub.cpp)
target_link_libraries(dds_dynamic_sub PRIVATE fastdds fastcdr)

# Microbenchmark de la Tarea 4 (RQ3): CPU estacionario de P participantes
# ociosos, para ajustar d en D = d*P(P-1)/2. Barrido: tools/participant_cost_sweep.py.
# ─────────────────────────────────────────────────────────────────────────────
# Instrumentación de la ruta de publicación (obligación 4 del Cap. 8).
# Por defecto OFF: el binario medido debe ser el binario publicado salvo este
# flag. Con OFF, PublishProbe es una clase vacía y el compilador la elimina.
# ─────────────────────────────────────────────────────────────────────────────
option(GATEWAY_INSTRUMENT "Instrumentar la ruta de publicación para RQ1" OFF)
if(GATEWAY_INSTRUMENT)
    target_compile_definitions(nmea_pipeline PUBLIC GATEWAY_INSTRUMENT)
    message(STATUS "Instrumentación de la ruta de publicación: ACTIVADA")
    add_executable(instrument_selftest tools/instrument_selftest.cpp)
    target_include_directories(instrument_selftest PRIVATE src)
    target_link_libraries(instrument_selftest PRIVATE
        nmea_pipeline nmea_mapper nmea_registry nmea_capture nmea_parser fastdds)
    target_compile_options(instrument_selftest PRIVATE -Wall -Wextra)
endif()

# Emite el IDL del brazo generado DESDE el registro, para que los dos brazos no
# puedan divergir. Se construye siempre: no depende de la instrumentacion.
add_executable(idl_from_registry tools/idl_from_registry.cpp)
target_include_directories(idl_from_registry PRIVATE src)
target_link_libraries(idl_from_registry PRIVATE nmea_registry)
target_compile_options(idl_from_registry PRIVATE -Wall -Wextra)

add_executable(participant_cost_bench tools/participant_cost_bench.cpp)
target_link_libraries(participant_cost_bench PRIVATE fastdds)
target_compile_options(participant_cost_bench PRIVATE -Wall -Wextra)

# ─────────────────────────────────────────────────────────────────────────────
# Interfaz de escritorio y arneses que instancian GatewayController (Qt + ROS).
# ─────────────────────────────────────────────────────────────────────────────
if(GATEWAY_UI_ENABLED AND GATEWAY_ROS_ENABLED)
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

    # Arneses headless que reutilizan GatewayController: necesitan Qt6::Core por
    # el bucle de eventos y señales, y nmea_ros porque el controlador publica en ROS.
    foreach(_harness test_multi_iface test_serial test_qos pub_hold rehearse_demos)
        add_executable(${_harness} tools/${_harness}.cpp src/ui/GatewayController.cpp)
        target_include_directories(${_harness} PRIVATE src)
        target_link_libraries(${_harness} PRIVATE
            nmea_pipeline nmea_mapper nmea_registry nmea_capture
            Qt6::Core fastdds nmea_ros)
    endforeach()

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
endif()

# Arnés headless: validación GPS→ROS (NavSatFix) por la ruta de producción.
if(GATEWAY_ROS_ENABLED)
    add_executable(verify_gps_ros tools/verify_gps_ros.cpp)
    target_include_directories(verify_gps_ros PRIVATE src)
    target_link_libraries(verify_gps_ros PRIVATE
        nmea_pipeline nmea_mapper nmea_registry nmea_capture fastdds nmea_ros)
endif()

# Pruebas. GoogleTest era obligatorio: sin él la configuración fallaba entera,
# aunque sólo se quisiera construir el núcleo. Ahora AUTO lo omite si falta.
if(GATEWAY_BUILD_TESTS STREQUAL "OFF")
    set(GATEWAY_TESTS_ENABLED OFF)
else()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        set(GATEWAY_TESTS_ENABLED ON)
    elseif(GATEWAY_BUILD_TESTS STREQUAL "ON")
        message(FATAL_ERROR
            "GATEWAY_BUILD_TESTS=ON pero no se encontró GoogleTest.\n"
            "  Instálelo (p. ej. apt install libgtest-dev) o use -DGATEWAY_BUILD_TESTS=OFF.")
    else()
        set(GATEWAY_TESTS_ENABLED OFF)
        message(STATUS "GoogleTest no encontrado: se omiten las pruebas. "
                       "Para exigirlas use -DGATEWAY_BUILD_TESTS=ON.")
    endif()
endif()

if(GATEWAY_TESTS_ENABLED)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "Puente ROS 2 : ${GATEWAY_ROS_BRIDGE} -> ${GATEWAY_ROS_ENABLED}")
message(STATUS "Interfaz Qt 6: ${GATEWAY_BUILD_UI} -> ${GATEWAY_UI_ENABLED}")
message(STATUS "Pruebas      : ${GATEWAY_BUILD_TESTS} -> ${GATEWAY_TESTS_ENABLED}")

# ─────────────────────────────────────────────────────────────────────────────
# Manifiesto de construcción.
#
# Se escribe en cada configuración con lo que realmente se resolvió: versiones,
# rutas, opciones y el commit exacto. Una medición cuyo entorno no está
# registrado no es reproducible, y depender de que alguien lo anote a mano no
# funciona. Este archivo se adjunta al manifiesto de cada corrida.
# ─────────────────────────────────────────────────────────────────────────────
string(TIMESTAMP GATEWAY_CONFIG_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)

# Los flags reales dependen del build type; registrarlos evita tener que
# adivinar después si una corrida se midió optimizada o no.
string(TOUPPER "${CMAKE_BUILD_TYPE}" _bt_upper)
set(GATEWAY_BUILD_TYPE_FLAGS "${CMAKE_CXX_FLAGS_${_bt_upper}}")

set(GATEWAY_GIT_SHA "desconocido")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --always --dirty --tags
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE GATEWAY_GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()

file(WRITE "${CMAKE_BINARY_DIR}/gateway_build_manifest.txt"
"# Manifiesto de construcción — generado por CMake, no editar a mano.\n"
"fecha_configuracion = ${GATEWAY_CONFIG_TIMESTAMP}\n"
"git_describe        = ${GATEWAY_GIT_SHA}\n"
"cmake               = ${CMAKE_VERSION}\n"
"generador           = ${CMAKE_GENERATOR}\n"
"compilador          = ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\n"
"compilador_ruta     = ${CMAKE_CXX_COMPILER}\n"
"cxx_standard        = ${CMAKE_CXX_STANDARD}\n"
"build_type          = ${CMAKE_BUILD_TYPE}\n"
"cxx_flags           = ${CMAKE_CXX_FLAGS}\n"
"cxx_flags_efectivos = ${CMAKE_CXX_FLAGS} ${GATEWAY_BUILD_TYPE_FLAGS}\n"
"sistema             = ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION} ${CMAKE_SYSTEM_PROCESSOR}\n"
"fastdds_dir         = ${fastdds_DIR}\n"
"fastdds_version     = ${fastdds_VERSION}\n"
"fastdds_prefix      = ${GATEWAY_FASTDDS_PREFIX}\n"
"gateway_instrument = ${GATEWAY_INSTRUMENT}\n"
"ros_bridge_opcion   = ${GATEWAY_ROS_BRIDGE}\n"
"ros_bridge_activo   = ${GATEWAY_ROS_ENABLED}\n"
"rclcpp_version      = ${rclcpp_VERSION}\n"
"ui_opcion           = ${GATEWAY_BUILD_UI}\n"
"ui_activa           = ${GATEWAY_UI_ENABLED}\n"
"qt6_version         = ${Qt6_VERSION}\n"
"runtime_lib_dirs    = ${GATEWAY_RUNTIME_LD_PATH}\n")
message(STATUS "Manifiesto de construcción: ${CMAKE_BINARY_DIR}/gateway_build_manifest.txt")
