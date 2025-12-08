# ==============================================================================
# ThermostatRTOS - Smart Thermostat Firmware
# ==============================================================================
#
# Copyright (c) 2024-2025 ThinkSense Labs
# Author: Gonzalo Patino
#
# SPDX-License-Identifier: MIT
#
# Project: ThermostatRTOS
# Description: Production-grade smart thermostat firmware for ESP32
# Platform: ESP32-WROOM-32
# Framework: ESP-IDF v5.5.1 + FreeRTOS
#
# ==============================================================================

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(thermostat
    VERSION 0.1.0
    DESCRIPTION "Smart Thermostat Firmware by ThinkSense Labs"
    LANGUAGES C
)
