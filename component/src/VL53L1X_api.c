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
#include <string.h>

const uint8_t VL51L1X_DEFAULT_CONFIGURATION[] = {
	0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x02, 0x08, 0x00, 0x08, 0x10, 0x01, 0x01, 0x00, 0x00, 0x00, 
	0x00, 0xff, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x0b, 0x00, 0x00, 0x02, 0x0a, 0x21, 
	0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x00, 0x00, 0x38, 0xff, 0x01, 0x00, 0x08, 0x00, 
	0x00, 0x01, 0xcc, 0x0f, 0x01, 0xf1, 0x0d, 0x01, 0x68, 0x00, 0x80, 0x08, 0xb8, 0x00, 0x00, 0x00, 
	0x00, 0x0f, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0f, 0x0d, 0x0e, 0x0e, 0x00, 
	0x00, 0x02, 0xc7, 0xff, 0x9B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00
};

static const uint8_t status_rtn[24] = {255, 255, 255, 5, 2, 4, 1, 7, 3, 0,
									   255, 255, 9, 13, 255, 255, 255, 255, 10, 6,
									   255, 255, 11, 12};

VL53L1X_ERROR VL53L1X_GetSWVersion(VL53L1X_Version_t *pVersion)
{
	pVersion->major = VL53L1X_IMPLEMENTATION_VER_MAJOR;
	pVersion->minor = VL53L1X_IMPLEMENTATION_VER_MINOR;
	pVersion->build = VL53L1X_IMPLEMENTATION_VER_SUB;
	pVersion->revision = VL53L1X_IMPLEMENTATION_VER_REVISION;
	return 0;
}

VL53L1X_ERROR VL53L1X_SetI2CAddress(uint16_t dev, uint8_t new_address)
{
	return VL53L1_WrByte(dev, VL53L1_I2C_SLAVE__DEVICE_ADDRESS, new_address);
}

VL53L1X_ERROR VL53L1X_SensorInit(uint16_t dev)
{
	VL53L1X_ERROR status = 0;
	uint8_t Addr = 0x00, tmp = 0;
	uint16_t timeout_counter = 0;

	for (Addr = 0x2D; Addr <= 0x87; Addr++)
	{
		status |= VL53L1_WrByte(dev, Addr, VL51L1X_DEFAULT_CONFIGURATION[Addr - 0x2D]);
	}
	status |= VL53L1X_StartRanging(dev);
	while (tmp == 0)
	{
		status = VL53L1X_CheckForDataReady(dev, &tmp);
		if (timeout_counter++ >= 1000) return (uint8_t)VL53L1X_ERROR_TIMEOUT;
		status = VL53L1_WaitMs(dev, 1);
	}
	status |= VL53L1X_ClearInterrupt(dev);
	status |= VL53L1X_StopRanging(dev);
	status |= VL53L1_WrByte(dev, VL53L1_VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND, 0x09);
	status |= VL53L1_WrByte(dev, 0x0B, 0);
	return status;
}

VL53L1X_ERROR VL53L1X_ClearInterrupt(uint16_t dev)
{
	return VL53L1_WrByte(dev, SYSTEM__INTERRUPT_CLEAR, 0x01);
}

VL53L1X_ERROR VL53L1X_SetInterruptPolarity(uint16_t dev, uint8_t NewPolarity)
{
	uint8_t Temp;
	VL53L1X_ERROR status = VL53L1_RdByte(dev, GPIO_HV_MUX__CTRL, &Temp);
	Temp = Temp & 0xEF;
	status |= VL53L1_WrByte(dev, GPIO_HV_MUX__CTRL, Temp | (!(NewPolarity & 1)) << 4);
	return status;
}

VL53L1X_ERROR VL53L1X_GetInterruptPolarity(uint16_t dev, uint8_t *pInterruptPolarity)
{
	uint8_t Temp;
	VL53L1X_ERROR status = VL53L1_RdByte(dev, GPIO_HV_MUX__CTRL, &Temp);
	*pInterruptPolarity = !( (Temp & 0x10) >> 4);
	return status;
}

VL53L1X_ERROR VL53L1X_StartRanging(uint16_t dev)
{
	return VL53L1_WrByte(dev, SYSTEM__MODE_START, 0x40);
}

VL53L1X_ERROR VL53L1X_StopRanging(uint16_t dev)
{
	return VL53L1_WrByte(dev, SYSTEM__MODE_START, 0x00);
}

VL53L1X_ERROR VL53L1X_CheckForDataReady(uint16_t dev, uint8_t *isDataReady)
{
	uint8_t Temp, IntPol;
	VL53L1X_ERROR status = VL53L1X_GetInterruptPolarity(dev, &IntPol);
	status |= VL53L1_RdByte(dev, GPIO__TIO_HV_STATUS, &Temp);
	if (status == 0) *isDataReady = ((Temp & 1) == IntPol) ? 1 : 0;
	return status;
}

VL53L1X_ERROR VL53L1X_SetTimingBudgetInMs(uint16_t dev, uint16_t TimingBudgetInMs)
{
	uint16_t DM;
	VL53L1X_ERROR status = VL53L1X_GetDistanceMode(dev, &DM);
	if (DM == 0) return 1;
	if (DM == 1) {
		switch (TimingBudgetInMs) {
			case 15: VL53L1_WrWord(dev, 0x005E, 0x001D); VL53L1_WrWord(dev, 0x0061, 0x0027); break;
			case 20: VL53L1_WrWord(dev, 0x005E, 0x0051); VL53L1_WrWord(dev, 0x0061, 0x006E); break;
			case 33: VL53L1_WrWord(dev, 0x005E, 0x00D6); VL53L1_WrWord(dev, 0x0061, 0x006E); break;
			case 50: VL53L1_WrWord(dev, 0x005E, 0x01AE); VL53L1_WrWord(dev, 0x0061, 0x01E8); break;
			case 100: VL53L1_WrWord(dev, 0x005E, 0x02E1); VL53L1_WrWord(dev, 0x0061, 0x0388); break;
			case 200: VL53L1_WrWord(dev, 0x005E, 0x03E1); VL53L1_WrWord(dev, 0x0061, 0x0496); break;
			case 500: VL53L1_WrWord(dev, 0x005E, 0x0591); VL53L1_WrWord(dev, 0x0061, 0x05C1); break;
			default: status = 1; break;
		}
	} else {
		switch (TimingBudgetInMs) {
			case 20: VL53L1_WrWord(dev, 0x005E, 0x001E); VL53L1_WrWord(dev, 0x0061, 0x0022); break;
			case 33: VL53L1_WrWord(dev, 0x005E, 0x0060); VL53L1_WrWord(dev, 0x0061, 0x006E); break;
			case 50: VL53L1_WrWord(dev, 0x005E, 0x00AD); VL53L1_WrWord(dev, 0x0061, 0x00C6); break;
			case 100: VL53L1_WrWord(dev, 0x005E, 0x01CC); VL53L1_WrWord(dev, 0x0061, 0x01EA); break;
			case 200: VL53L1_WrWord(dev, 0x005E, 0x02D9); VL53L1_WrWord(dev, 0x0061, 0x02F8); break;
			case 500: VL53L1_WrWord(dev, 0x005E, 0x048F); VL53L1_WrWord(dev, 0x0061, 0x04A4); break;
			default: status = 1; break;
		}
	}
	return status;
}

VL53L1X_ERROR VL53L1X_GetTimingBudgetInMs(uint16_t dev, uint16_t *pTimingBudget)
{
	uint16_t Temp;
	VL53L1X_ERROR status = VL53L1_RdWord(dev, 0x005E, &Temp);
	switch (Temp) {
		case 0x001D: *pTimingBudget = 15; break;
		case 0x0051: case 0x001E: *pTimingBudget = 20; break;
		case 0x00D6: case 0x0060: *pTimingBudget = 33; break;
		case 0x1AE: case 0x00AD: *pTimingBudget = 50; break;
		case 0x02E1: case 0x01CC: *pTimingBudget = 100; break;
		case 0x03E1: case 0x02D9: *pTimingBudget = 200; break;
		case 0x0591: case 0x048F: *pTimingBudget = 500; break;
		default: status = 1; *pTimingBudget = 0;
	}
	return status;
}

VL53L1X_ERROR VL53L1X_SetDistanceMode(uint16_t dev, uint16_t DM)
{
	uint16_t TB;
	VL53L1X_ERROR status = VL53L1X_GetTimingBudgetInMs(dev, &TB);
	if (status != 0) return 1;
	switch (DM) {
		case 1:
			status = VL53L1_WrByte(dev, 0x004B, 0x14); status |= VL53L1_WrByte(dev, 0x0060, 0x07);
			status |= VL53L1_WrByte(dev, 0x0063, 0x05); status |= VL53L1_WrByte(dev, 0x0069, 0x38);
			status |= VL53L1_WrWord(dev, 0x0078, 0x0705); status |= VL53L1_WrWord(dev, 0x007A, 0x0606);
			break;
		case 2:
			status = VL53L1_WrByte(dev, 0x004B, 0x0A); status |= VL53L1_WrByte(dev, 0x0060, 0x0F);
			status |= VL53L1_WrByte(dev, 0x0063, 0x0D); status |= VL53L1_WrByte(dev, 0x0069, 0xB8);
			status |= VL53L1_WrWord(dev, 0x0078, 0x0F0D); status |= VL53L1_WrWord(dev, 0x007A, 0x0E0E);
			break;
		default: status = 1; break;
	}
	if (status == 0) status |= VL53L1X_SetTimingBudgetInMs(dev, TB);
	return status;
}

VL53L1X_ERROR VL53L1X_GetDistanceMode(uint16_t dev, uint16_t *DM)
{
	uint8_t TempDM;
	VL53L1X_ERROR status = VL53L1_RdByte(dev, 0x004B, &TempDM);
	if (TempDM == 0x14) *DM = 1;
	if (TempDM == 0x0A) *DM = 2;
	return status;
}

VL53L1X_ERROR VL53L1X_SetInterMeasurementInMs(uint16_t dev, uint32_t InterMeasMs)
{
	uint16_t ClockPLL;
	VL53L1X_ERROR status = VL53L1_RdWord(dev, 0x00DE, &ClockPLL);
	ClockPLL = ClockPLL & 0x3FF;
	VL53L1_WrDWord(dev, 0x006C, (uint32_t)(ClockPLL * InterMeasMs * 1.075));
	return status;
}

VL53L1X_ERROR VL53L1X_GetInterMeasurementInMs(uint16_t dev, uint16_t *pIM)
{
	uint16_t ClockPLL;
	uint32_t tmp;
	VL53L1X_ERROR status = VL53L1_RdDWord(dev, 0x006C, &tmp);
	*pIM = (uint16_t)tmp;
	status |= VL53L1_RdWord(dev, 0x00DE, &ClockPLL);
	ClockPLL = ClockPLL & 0x3FF;
	*pIM = (uint16_t)(*pIM / (ClockPLL * 1.065));
	return status;
}

VL53L1X_ERROR VL53L1X_BootState(uint16_t dev, uint8_t *state)
{
	return VL53L1_RdByte(dev, 0x00E5, state);
}

VL53L1X_ERROR VL53L1X_GetSensorId(uint16_t dev, uint16_t *sensorId)
{
	return VL53L1_RdWord(dev, 0x010F, sensorId);
}

VL53L1X_ERROR VL53L1X_GetDistance(uint16_t dev, uint16_t *distance)
{
	return VL53L1_RdWord(dev, 0x0096, distance);
}

VL53L1X_ERROR VL53L1X_GetSignalPerSpad(uint16_t dev, uint16_t *signalRate)
{
	uint16_t SpNb = 1, signal;
	VL53L1X_ERROR status = VL53L1_RdWord(dev, 0x0098, &signal);
	status |= VL53L1_RdWord(dev, 0x008C, &SpNb);
	*signalRate = (uint16_t)(200.0 * signal / SpNb);
	return status;
}

VL53L1X_ERROR VL53L1X_GetAmbientPerSpad(uint16_t dev, uint16_t *ambPerSp)
{
	uint16_t AmbientRate, SpNb = 1;
	VL53L1X_ERROR status = VL53L1_RdWord(dev, 0x0090, &AmbientRate);
	status |= VL53L1_RdWord(dev, 0x008C, &SpNb);
	*ambPerSp = (uint16_t)(200.0 * AmbientRate / SpNb);
	return status;
}

VL53L1X_ERROR VL53L1X_GetSignalRate(uint16_t dev, uint16_t *signal)
{
	uint16_t tmp;
	VL53L1X_ERROR status = VL53L1_RdWord(dev, 0x0098, &tmp);
	*signal = tmp * 8;
	return status;
}

VL53L1X_ERROR VL53L1X_GetSpadNb(uint16_t dev, uint16_t *spNb)
{
	uint16_t tmp;
	VL53L1X_ERROR status = VL53L1_RdWord(dev, 0x008C, &tmp);
	*spNb = tmp >> 8;
	return status;
}

VL53L1X_ERROR VL53L1X_GetAmbientRate(uint16_t dev, uint16_t *ambRate)
{
	uint16_t tmp;
	VL53L1X_ERROR status = VL53L1_RdWord(dev, 0x0090, &tmp);
	*ambRate = tmp * 8;
	return status;
}

VL53L1X_ERROR VL53L1X_GetRangeStatus(uint16_t dev, uint8_t *rangeStatus)
{
	uint8_t RgSt;
	VL53L1X_ERROR status = VL53L1_RdByte(dev, 0x0089, &RgSt);
	RgSt = RgSt & 0x1F;
	*rangeStatus = (RgSt < 24) ? status_rtn[RgSt] : 255;
	return status;
}

VL53L1X_ERROR VL53L1X_GetResult(uint16_t dev, VL53L1X_Result_t *pResult)
{
	uint8_t Temp[17], RgSt;
	VL53L1X_ERROR status = VL53L1_ReadMulti(dev, 0x0089, Temp, 17);
	RgSt = Temp[0] & 0x1F;
	pResult->Status = (RgSt < 24) ? status_rtn[RgSt] : 255;
	pResult->Ambient = (Temp[7] << 8 | Temp[8]) * 8;
	pResult->NumSPADs = Temp[3];
	pResult->SigPerSPAD = (Temp[15] << 8 | Temp[16]) * 8;
	pResult->Distance = Temp[13] << 8 | Temp[14];
	return status;
}

VL53L1X_ERROR VL53L1X_SetOffset(uint16_t dev, int16_t OffsetValue)
{
	VL53L1X_ERROR status = VL53L1_WrWord(dev, 0x001E, (uint16_t)(OffsetValue * 4));
	status |= VL53L1_WrWord(dev, 0x0020, 0x0);
	status |= VL53L1_WrWord(dev, 0x0022, 0x0);
	return status;
}

VL53L1X_ERROR VL53L1X_GetOffset(uint16_t dev, int16_t *offset)
{
	uint16_t Temp;
	VL53L1X_ERROR status = VL53L1_RdWord(dev, 0x001E, &Temp);
	Temp = (Temp << 3) >> 5;
	*offset = (int16_t)Temp;
	if (*offset > 1024) {
		*offset -= 2048;
	}
	return status;
}

VL53L1X_ERROR VL53L1X_SetXtalk(uint16_t dev, uint16_t XtalkValue)
{
	VL53L1X_ERROR status = VL53L1_WrWord(dev, 0x0018, 0x0000);
	status |= VL53L1_WrWord(dev, 0x001A, 0x0000);
	status |= VL53L1_WrWord(dev, 0x0016, (XtalkValue << 9) / 1000);
	return status;
}

VL53L1X_ERROR VL53L1X_GetXtalk(uint16_t dev, uint16_t *xtalk)
{
	uint16_t tmp;
	VL53L1X_ERROR status = VL53L1_RdWord(dev, 0x0016, &tmp);
	*xtalk = (uint16_t)((tmp * 1000) >> 9);
	return status;
}

VL53L1X_ERROR VL53L1X_SetDistanceThreshold(uint16_t dev, uint16_t ThreshLow, uint16_t ThreshHigh, uint8_t Window, uint8_t IntOnNoTarget)
{
	uint8_t Temp;
	VL53L1X_ERROR status = VL53L1_RdByte(dev, 0x0046, &Temp);
	Temp = (Temp & ~0x6F) | Window;
	status |= VL53L1_WrByte(dev, 0x0046, (IntOnNoTarget == 0) ? Temp : (Temp | 0x40));
	status |= VL53L1_WrWord(dev, 0x0072, ThreshHigh);
	status |= VL53L1_WrWord(dev, 0x0074, ThreshLow);
	return status;
}

VL53L1X_ERROR VL53L1X_GetDistanceThresholdWindow(uint16_t dev, uint16_t *window)
{
	uint8_t tmp;
	VL53L1X_ERROR status = VL53L1_RdByte(dev, 0x0046, &tmp);
	*window = (uint16_t)(tmp & 0x7);
	return status;
}

VL53L1X_ERROR VL53L1X_GetDistanceThresholdLow(uint16_t dev, uint16_t *low)
{
	return VL53L1_RdWord(dev, 0x0074, low);
}

VL53L1X_ERROR VL53L1X_GetDistanceThresholdHigh(uint16_t dev, uint16_t *high)
{
	return VL53L1_RdWord(dev, 0x0072, high);
}

VL53L1X_ERROR VL53L1X_SetROICenter(uint16_t dev, uint8_t ROICenter)
{
	return VL53L1_WrByte(dev, 0x007F, ROICenter);
}

VL53L1X_ERROR VL53L1X_GetROICenter(uint16_t dev, uint8_t *ROICenter)
{
	return VL53L1_RdByte(dev, 0x007F, ROICenter);
}

VL53L1X_ERROR VL53L1X_SetROI(uint16_t dev, uint16_t X, uint16_t Y)
{
	uint8_t OpticalCenter;
	VL53L1X_ERROR status = VL53L1_RdByte(dev, 0x013E, &OpticalCenter);
	if (X > 16) {
		X = 16;
	}
	if (Y > 16) {
		Y = 16;
	}
	if (X > 10 || Y > 10) OpticalCenter = 199;
	status |= VL53L1_WrByte(dev, 0x007F, OpticalCenter);
	status |= VL53L1_WrByte(dev, 0x0080, (Y - 1) << 4 | (X - 1));
	return status;
}

VL53L1X_ERROR VL53L1X_GetROI_XY(uint16_t dev, uint16_t *ROI_X, uint16_t *ROI_Y)
{
	uint8_t tmp;
	VL53L1X_ERROR status = VL53L1_RdByte(dev, 0x0080, &tmp);
	*ROI_X = (tmp & 0x0F) + 1; *ROI_Y = ((tmp & 0xF0) >> 4) + 1;
	return status;
}

VL53L1X_ERROR VL53L1X_SetSignalThreshold(uint16_t dev, uint16_t Signal)
{
	return VL53L1_WrWord(dev, 0x0066, Signal >> 3);
}

VL53L1X_ERROR VL53L1X_GetSignalThreshold(uint16_t dev, uint16_t *signal)
{
	uint16_t tmp;
	VL53L1X_ERROR status = VL53L1_RdWord(dev, 0x0066, &tmp);
	*signal = tmp << 3;
	return status;
}

VL53L1X_ERROR VL53L1X_SetSigmaThreshold(uint16_t dev, uint16_t Sigma)
{
	if (Sigma > (0xFFFF >> 2)) return 1;
	return VL53L1_WrWord(dev, 0x0064, Sigma << 2);
}

VL53L1X_ERROR VL53L1X_GetSigmaThreshold(uint16_t dev, uint16_t *sigma)
{
	uint16_t tmp;
	VL53L1X_ERROR status = VL53L1_RdWord(dev, 0x0064, &tmp);
	*sigma = tmp >> 2;
	return status;
}

VL53L1X_ERROR VL53L1X_StartTemperatureUpdate(uint16_t dev)
{
	VL53L1X_ERROR status = 0;
	uint8_t tmp = 0;
	uint16_t timeout_counter = 0;
	status |= VL53L1_WrByte(dev, 0x0008, 0x81);
	status |= VL53L1_WrByte(dev, 0x0B, 0x92);
	status |= VL53L1X_StartRanging(dev);
	while (tmp == 0) {
		status = VL53L1X_CheckForDataReady(dev, &tmp);
		if (timeout_counter++ >= 1000) {
			return (uint8_t)VL53L1X_ERROR_TIMEOUT;
		}
		status = VL53L1_WaitMs(dev, 1);
	}
	status |= VL53L1X_ClearInterrupt(dev);
	status |= VL53L1X_StopRanging(dev);
	status |= VL53L1_WrByte(dev, 0x0008, 0x09);
	status |= VL53L1_WrByte(dev, 0x0B, 0);
	return status;
}
