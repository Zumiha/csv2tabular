cmake_minimum_required(VERSION 3.20.0)
project(Utils VERSION 0.1 LANGUAGES CXX)
set(SOURCE Utils.h)
add_library(${PROJECT_NAME} STATIC ${SOURCE})