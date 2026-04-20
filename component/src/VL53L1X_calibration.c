/**
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include "VL53L1X_api.h"
#include "VL53L1X_calibration.h"

int8_t VL53L1X_CalibrateOffset(uint16_t dev, uint16_t TargetDistInMm, int16_t *offset)
{
	uint8_t i, tmp=0;
	int16_t AverageDistance = 0;
	uint16_t distance;
	VL53L1X_ERROR status = 0;
	uint16_t timeout_counter =0;

	status |= VL53L1_WrWord(dev, 0x001E, 0x0);
	status |= VL53L1_WrWord(dev, 0x0020, 0x0);
	status |= VL53L1_WrWord(dev, 0x0022, 0x0);
	status |= VL53L1X_StartRanging(dev);
	for (i = 0; i < 50; i++) {
		while (tmp == 0)
		{
			status = VL53L1X_CheckForDataReady(dev, &tmp);
			if (timeout_counter++ >= 1000) {
				return (uint8_t)VL53L1X_ERROR_TIMEOUT;
			}
			status = VL53L1_WaitMs(dev, 1);
		}
		tmp = 0;
		timeout_counter=0;
		status |= VL53L1X_GetDistance(dev, &distance);
		status |= VL53L1X_ClearInterrupt(dev);
		AverageDistance = AverageDistance + distance;
	}
	status |= VL53L1X_StopRanging(dev);
	AverageDistance = AverageDistance / 50;
	*offset = TargetDistInMm - AverageDistance;
	status |= VL53L1_WrWord(dev, 0x001E, *offset*4);
	return status;
}

int8_t VL53L1X_CalibrateXtalk(uint16_t dev, uint16_t TargetDistInMm, uint16_t *xtalk)
{
	uint8_t i, tmp=0;
	float AverageSignalRate = 0;
	float AverageDistance = 0;
	float AverageSpadNb = 0;
	uint16_t distance = 0, spadNum, sr;
	VL53L1X_ERROR status = 0;
	uint32_t calXtalk;
	uint16_t timeout_counter =0;

	status |= VL53L1_WrWord(dev, 0x0016, 0);
	status |= VL53L1X_StartRanging(dev);
	for (i = 0; i < 50; i++) {
		while (tmp == 0)
		{
			status = VL53L1X_CheckForDataReady(dev, &tmp);
			if (timeout_counter++ >= 1000) {
				return (uint8_t)VL53L1X_ERROR_TIMEOUT;
			}
			status = VL53L1_WaitMs(dev, 1);
		}
		tmp = 0;
		timeout_counter=0;
		status |= VL53L1X_GetSignalRate(dev, &sr);
		status |= VL53L1X_GetDistance(dev, &distance);
		status |= VL53L1X_ClearInterrupt(dev);
		AverageDistance = AverageDistance + distance;
		status = VL53L1X_GetSpadNb(dev, &spadNum);
		AverageSpadNb = AverageSpadNb + spadNum;
		AverageSignalRate = AverageSignalRate + sr;
	}
	status |= VL53L1X_StopRanging(dev);
	AverageDistance = AverageDistance / 50;
	AverageSpadNb = AverageSpadNb / 50;
	AverageSignalRate = AverageSignalRate / 50;
	calXtalk = (uint16_t)(512*(AverageSignalRate*(1-(AverageDistance/TargetDistInMm)))/AverageSpadNb);
	if (calXtalk > 0xffff) {
		calXtalk = 0xffff;
	}
	*xtalk = (uint16_t)((calXtalk*1000)>>9);
	status |= VL53L1_WrWord(dev, 0x0016, (uint16_t)calXtalk);
	return status;
}
