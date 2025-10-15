cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Indicamos explícitamente dónde está el componente principal
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/main_code")

project(proyecto_riego)
