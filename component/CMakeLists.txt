idf_component_register(
    SRCS 
        "src/i2c.c"
        "src/pcf8575.c"
        "src/TB6612FNG.c"
        "src/spi.c"
        "src/joystick.c"
        # --- TẠM ẨN: VL53L1X ---
        # "src/vl53l1x.c"
        # "src/VL53L1X_api.c"
        # "src/VL53L1X_calibration.c"
        # "src/vl53l1_platform.c"
        # --- TẠM ẨN: BNO08x ---
        # "src/esp32_bno08x.c"
        # "src/BNO08x.cpp"
        # "src/BNO08xRpt.cpp"
        # "src/BNO08xSH2HAL.cpp"
        # "src/report/BNO08xRptAcceleration.cpp"
        # "src/report/BNO08xRptActivityClassifier.cpp"
        # "src/report/BNO08xRptARVRStabilizedGameRV.cpp"
        # "src/report/BNO08xRptARVRStabilizedRV.cpp"
        # "src/report/BNO08xRptCalGyro.cpp"
        # "src/report/BNO08xRptCalMagnetometer.cpp"
        # "src/report/BNO08xRptGameRV.cpp"
        # "src/report/BNO08xRptGravity.cpp"
        # "src/report/BNO08xRptIGyroRV.cpp"
        # "src/report/BNO08xRptLinearAcceleration.cpp"
        # "src/report/BNO08xRptRawMEMSAccelerometer.cpp"
        # "src/report/BNO08xRptRawMEMSGyro.cpp"
        # "src/report/BNO08xRptRawMEMSMagnetometer.cpp"
        # "src/report/BNO08xRptRV.cpp"
        # "src/report/BNO08xRptRVGeneric.cpp"
        # "src/report/BNO08xRptRVGeomag.cpp"
        # "src/report/BNO08xRptShakeDetector.cpp"
        # "src/report/BNO08xRptStabilityClassifier.cpp"
        # "src/report/BNO08xRptStepCounter.cpp"
        # "src/report/BNO08xRptTapDetector.cpp"
        # "src/report/BNO08xRptUncalGyro.cpp"
        # "src/report/BNO08xRptUncalMagnetometer.cpp"
        # "include/SH2/sh2.c"
        # "include/SH2/sh2_SensorValue.c"
        # "include/SH2/sh2_util.c"
        # "include/SH2/shtp.c"
    INCLUDE_DIRS 
        "include"
        "include/report"
        "include/callback"
        "include/SH2"
    REQUIRES 
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_spi
        esp_driver_ledc
        esp_driver_pcnt
        driver
        esp_timer
        cmock
)
# --- TẠM ẨN: BNO08x CXX properties ---
# set_source_files_properties(
#     src/esp32_bno08x.c
#     src/BNO08x.cpp
#     src/BNO08xRpt.cpp
#     src/BNO08xSH2HAL.cpp
#     src/report/BNO08xRptAcceleration.cpp
#     src/report/BNO08xRptActivityClassifier.cpp
#     src/report/BNO08xRptARVRStabilizedGameRV.cpp
#     src/report/BNO08xRptARVRStabilizedRV.cpp
#     src/report/BNO08xRptCalGyro.cpp
#     src/report/BNO08xRptCalMagnetometer.cpp
#     src/report/BNO08xRptGameRV.cpp
#     src/report/BNO08xRptGravity.cpp
#     src/report/BNO08xRptIGyroRV.cpp
#     src/report/BNO08xRptLinearAcceleration.cpp
#     src/report/BNO08xRptRawMEMSAccelerometer.cpp
#     src/report/BNO08xRptRawMEMSGyro.cpp
#     src/report/BNO08xRptRawMEMSMagnetometer.cpp
#     src/report/BNO08xRptRV.cpp
#     src/report/BNO08xRptRVGeneric.cpp
#     src/report/BNO08xRptRVGeomag.cpp
#     src/report/BNO08xRptShakeDetector.cpp
#     src/report/BNO08xRptStabilityClassifier.cpp
#     src/report/BNO08xRptStepCounter.cpp
#     src/report/BNO08xRptTapDetector.cpp
#     src/report/BNO08xRptUncalGyro.cpp
#     src/report/BNO08xRptUncalMagnetometer.cpp
#     PROPERTIES LANGUAGE CXX
# )
