/*
 * --------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <p-sam@d3vs.net>, <natinusala@gmail.com>, <m4x@m4xw.net>
 * wrote this file. As long as you retain this notice you can do whatever you
 * want with this stuff. If you meet any of us some day, and you think this
 * stuff is worth it, you can buy us a beer in return.  - The sys-clk authors
 * --------------------------------------------------------------------------
 */

#pragma once

#include <stdint.h>
#include "clocks.h"

#define API_VERSION 4
#define IPC_SERVICE_NAME "4IFIR" // 4IFIR

enum SysClkIpcCmd
{
    IpcCmd_GetApiVersion = 0,
    IpcCmd_GetVersionString = 1,
    IpcCmd_GetCurrentContext = 2,
    IpcCmd_Exit = 3,
    IpcCmd_GetProfileCount = 4,
    IpcCmd_GetProfiles = 5,
    IpcCmd_SetProfiles = 6,
    IpcCmd_SetEnabled = 7,
    IpcCmd_SetOverride = 8,
    IpcCmd_GetConfigValues = 9,
    IpcCmd_SetConfigValues = 10,
    IpcCmd_SetReverseNXRTMode = 11,
    IpcCmd_GetFrequencyTable = 12,
    IpcCmd_GetIsMariko = 13,
    IpcCmd_GetBatteryChargingDisabledOverride = 14,
    IpcCmd_SetBatteryChargingDisabledOverride = 15,
    IpcCmd_GetCustomValue = 16,
    IpcCmd_GetIsOled = 17,
    IpcCmd_GetPrismEnabled = 18,
    IpcCmd_SetPrismEnabled = 19,
    IpcCmd_GetPrismMode = 20,
    IpcCmd_SetPrismMode = 21,
    IpcCmd_GetMemTimings = 22,
    IpcCmd_SetMemTimings = 23,
	// SaltyNX integration commands
    IpcCmd_SaltyNX_GetConnected = 24,
    IpcCmd_SaltyNX_SetDisplayRefreshRate = 25,        // DEPRECATED: Use direct docked_refresh_rate.cpp implementation
    IpcCmd_SaltyNX_GetDisplayRefreshRate = 26,        // DEPRECATED: Use direct docked_refresh_rate.cpp implementation
    IpcCmd_SaltyNX_SetDisplaySync = 27,               // DEPRECATED: Use direct docked_refresh_rate.cpp implementation
    IpcCmd_SaltyNX_SetDockedRefreshRates = 28,        // DEPRECATED: Use direct docked_refresh_rate.cpp implementation
    IpcCmd_SaltyNX_SetDontForce60InDocked = 29,       // DEPRECATED: Use direct docked_refresh_rate.cpp implementation
    IpcCmd_SaltyNX_SetMatchLowestRR = 30,             // Active: Used for render interval control
	// NxFps shared memory commands
    IpcCmd_NxFps_GetStatus = 31,
    IpcCmd_NxFps_SetFPSLocked = 32,
    IpcCmd_NxFps_SetZeroSync = 33,
    IpcCmd_NxFps_GetFPSInfo = 34,
    // Fuel Gauge commands
    IpcCmd_GetFuelGaugeInfo = 35,        // Get FullCap, DesignCap, Cycles, Age
    IpcCmd_ApplyBatteryCapacity = 36,    // Apply capacity scale setting now
};

// Fuel Gauge info structure
typedef struct {
    uint32_t fullCap;       // Current FullCap in mAh
    uint32_t designCap;     // Design capacity in mAh
    uint32_t fullCapNom;    // Nominal full capacity in mAh
    uint32_t cycles;        // Charge cycle count
    uint32_t age;           // Battery age percentage (FullCap/DesignCap * 100)
} FuelGaugeInfo;

typedef struct
{
    uint64_t tid;
    SysClkTitleProfileList profiles;
} Ipc_SetProfiles_Args;

typedef struct
{
    Module module;
    uint32_t hz;
} Ipc_SetOverride_Args;

typedef struct
{
    Module module;
    SysClkProfile profile;
} Ipc_GetFrequencyTable_Args;