cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS ../../components/ static_images $ENV{IDF_PATH}/examples/common_components/protocol_examples_common $ENV{IDF_PATH}/components/fatfs $ENV{IDF_PATH}/components/sdmmc $ENV{IDF_PATH}/components/driver)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(person_detection)
