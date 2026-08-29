# GTest ya fue localizado en el CMakeLists.txt raíz, que decide si este
# subdirectorio se añade siquiera.
# LD_LIBRARY_PATH se deriva del prefijo real donde se encontró Fast DDS
# (GATEWAY_RUNTIME_LD_PATH, calculado en el CMakeLists.txt raíz) en lugar de
# estar escrito a mano como /opt/ros/lyrical/lib. Con la ruta fija, las pruebas
# sólo se ejecutaban en una máquina.
set(_dds_env "LD_LIBRARY_PATH=${GATEWAY_RUNTIME_LD_PATH}")

add_executable(parser_test parser_test.cpp)
target_link_libraries(parser_test PRIVATE nmea_parser GTest::gtest GTest::gtest_main)
add_test(NAME parser_test COMMAND parser_test)

add_executable(registry_test registry_test.cpp)
target_link_libraries(registry_test PRIVATE nmea_registry GTest::gtest GTest::gtest_main)
add_test(NAME registry_test COMMAND registry_test)

add_executable(pipeline_test pipeline_test.cpp)
target_link_libraries(pipeline_test PRIVATE nmea_pipeline GTest::gtest GTest::gtest_main)
add_test(NAME pipeline_test COMMAND pipeline_test)
set_tests_properties(pipeline_test PROPERTIES
    ENVIRONMENT "${_dds_env}"
    TIMEOUT 30)

add_executable(capture_test capture_test.cpp)
target_link_libraries(capture_test PRIVATE nmea_capture GTest::gtest GTest::gtest_main)
add_test(NAME capture_test COMMAND capture_test)

add_executable(mapper_test mapper_test.cpp)
target_link_libraries(mapper_test PRIVATE nmea_mapper GTest::gtest GTest::gtest_main)
add_test(NAME mapper_test COMMAND mapper_test)
set_tests_properties(mapper_test PROPERTIES
    ENVIRONMENT "${_dds_env}")

add_executable(qos_recommender_test qos_recommender_test.cpp
               ${CMAKE_SOURCE_DIR}/src/ui/QoSRecommender.cpp)
target_include_directories(qos_recommender_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(qos_recommender_test PRIVATE
    nmea_registry GTest::gtest GTest::gtest_main)
add_test(NAME qos_recommender_test COMMAND qos_recommender_test)

add_executable(publish_plan_test publish_plan_test.cpp)
target_link_libraries(publish_plan_test PRIVATE nmea_pipeline GTest::gtest GTest::gtest_main)
add_test(NAME publish_plan_test COMMAND publish_plan_test)
set_tests_properties(publish_plan_test PROPERTIES
    ENVIRONMENT "${_dds_env}")

# Sólo tiene sentido si el puente ROS se construyó.
if(GATEWAY_ROS_ENABLED)
    add_executable(ros_publisher_test ros_publisher_test.cpp)
    target_include_directories(ros_publisher_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(ros_publisher_test PRIVATE nmea_ros GTest::gtest GTest::gtest_main)
    add_test(NAME ros_publisher_test COMMAND ros_publisher_test)
    set_tests_properties(ros_publisher_test PROPERTIES
        ENVIRONMENT "${_dds_env}"
        TIMEOUT 30)
endif()
