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

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
//#include <utils/types.h>

#include "../config.h"
#include "../clocks.h"
#include "../ipc.h"

bool IpcRunning();
Result IpcInitialize(void);
void IpcExit(void);

Result IpcGetAPIVersion(u32* out_ver);
Result IpcGetVersionString(char* out, size_t len);
Result IpcGetCurrentContext(SysClkContext* out_context);
Result IpcGetProfileCount(u64 tid, u8* out_count);
Result IpcSetEnabled(bool enabled);
Result IpcExitCmd();
Result IpcSetOverride(Module module, u32 hz);
Result IpcGetProfiles(u64 tid, SysClkTitleProfileList* out_profiles);
Result IpcSetProfiles(u64 tid, SysClkTitleProfileList* profiles);
Result IpcGetConfigValues(ConfigValueList* out_configValues);
Result IpcSetConfigValues(ConfigValueList* configValues);
Result IpcSetReverseNXRTMode(ReverseNXMode mode);
Result IpcGetFrequencyTable(Module module, SysClkProfile profile, SysClkFrequencyTable* out_table);
Result IpcGetIsMariko(bool* out_is_mariko);
Result IpcGetIsOled(bool* out_is_oled);
Result IpcGetBatteryChargingDisabledOverride(bool* out_is_true);
Result IpcSetBatteryChargingDisabledOverride(bool toggle_true);
Result IpcGetCustomValue(uint32_t index, uint32_t* out_value);
Result IpcGetPrismEnabled(bool* out_enabled);
Result IpcSetPrismEnabled(bool enabled);
Result IpcGetPrismMode(SysClkPrismMode* out_mode);
Result IpcSetPrismMode(SysClkPrismMode mode);
Result IpcGetMemTimings(Timing timing, uint32_t* out_value);
Result IpcSetMemTimings(Timing timing, uint32_t value);
Result IpcGetFuelGaugeInfo(FuelGaugeInfo* out_info);
Result IpcApplyBatteryCapacity(void);

static inline Result IpcRemoveOverride(Module module)
{
    return IpcSetOverride(module, 0);
}

#ifdef __cplusplus
}
#endif