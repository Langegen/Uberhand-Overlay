/*
 * --------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <p-sam@d3vs.net>, <natinusala@gmail.com>, <m4x@m4xw.net>
 * wrote this file. As long as you retain this notice you can do whatever you
 * want with this stuff. If you meet any of us some day, and you think this
 * stuff is worth it, you can buy us a beer in return.  - The sys-clk authors
 * --------------------------------------------------------------------------
 */

#define NX_SERVICE_ASSUME_NON_DOMAIN
#include <sysclk/client/ipc.h>
#include <switch.h>
#include <string.h>
#include <stdatomic.h>

static Service g_sysclkSrv;
static atomic_size_t g_refCnt;

bool IpcRunning()
{
    Handle handle;
    bool running = R_FAILED(smRegisterService(&handle, smEncodeName(IPC_SERVICE_NAME), false, 1));

    if (!running)
    {
        smUnregisterService(smEncodeName(IPC_SERVICE_NAME));
    }

  return running;
}

Result IpcInitialize(void)
{
    Result rc = 0;

    g_refCnt++;

    if (serviceIsActive(&g_sysclkSrv))
        return 0;

    rc = smGetService(&g_sysclkSrv, IPC_SERVICE_NAME);

    if (R_FAILED(rc)) IpcExit();

    return rc;
}

void IpcExit(void)
{
    if (--g_refCnt == 0)
    {
        serviceClose(&g_sysclkSrv);
    }
}

Result IpcGetAPIVersion(u32* out_ver)
{
    return serviceDispatchOut(&g_sysclkSrv, IpcCmd_GetApiVersion, *out_ver);
}

Result IpcGetVersionString(char* out, size_t len)
{
    return serviceDispatch(&g_sysclkSrv, IpcCmd_GetVersionString,
        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
        .buffers = {{out, len}},
    );
}
/*
Result IpcGetCurrentContext(SysClkContext* out_context)
{
    return serviceDispatchOut(&g_sysclkSrv, IpcCmd_GetCurrentContext, *out_context);
}
*/
Result IpcGetCurrentContext(SysClkContext* out_context)
{
    return serviceDispatch(&g_sysclkSrv, IpcCmd_GetCurrentContext,
        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
        .buffers = {{out_context, sizeof(SysClkContext)}},
    );
}

Result IpcGetProfileCount(u64 tid, u8* out_count)
{
    return serviceDispatchInOut(&g_sysclkSrv, IpcCmd_GetProfileCount, tid, *out_count);
}

Result IpcSetEnabled(bool enabled)
{
    u8 enabledRaw = (u8)enabled;
    return serviceDispatchIn(&g_sysclkSrv, IpcCmd_SetEnabled, enabledRaw);
}

Result IpcSetOverride(Module module, u32 hz)
{
    Ipc_SetOverride_Args args = {
        .module = module,
        .hz = hz
    };
    return serviceDispatchIn(&g_sysclkSrv, IpcCmd_SetOverride, args);
}

Result IpcGetProfiles(u64 tid, SysClkTitleProfileList* out_profiles)
{
    return serviceDispatchInOut(&g_sysclkSrv, IpcCmd_GetProfiles, tid, *out_profiles);
}

Result IpcSetProfiles(u64 tid, SysClkTitleProfileList* profiles)
{
    Ipc_SetProfiles_Args args;
    args.tid = tid;
    memcpy(&args.profiles, profiles, sizeof(SysClkTitleProfileList));
    return serviceDispatchIn(&g_sysclkSrv, IpcCmd_SetProfiles, args);
}

Result IpcGetConfigValues(ConfigValueList* out_configValues)
{
    // Use buffer transfer instead of inline for large ConfigValueList (>224 bytes)
    return serviceDispatch(&g_sysclkSrv, IpcCmd_GetConfigValues,
        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
        .buffers = {{out_configValues, sizeof(ConfigValueList)}},
    );
}

Result IpcSetConfigValues(ConfigValueList* configValues)
{
    // Use buffer transfer instead of inline for large ConfigValueList (>224 bytes)
    return serviceDispatch(&g_sysclkSrv, IpcCmd_SetConfigValues,
        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In },
        .buffers = {{configValues, sizeof(ConfigValueList)}},
    );
}

Result IpcSetReverseNXRTMode(ReverseNXMode mode)
{
    return serviceDispatchIn(&g_sysclkSrv, IpcCmd_SetReverseNXRTMode, mode);
}

Result IpcGetFrequencyTable(Module module, SysClkProfile profile, SysClkFrequencyTable* out_table)
{
    Ipc_GetFrequencyTable_Args args = {
        .module = module,
        .profile = profile,
    };
    return serviceDispatchInOut(&g_sysclkSrv, IpcCmd_GetFrequencyTable, args, *out_table);
}

Result IpcGetIsMariko(bool* out_is_mariko)
{
    return serviceDispatchOut(&g_sysclkSrv, IpcCmd_GetIsMariko, *out_is_mariko);
}

Result IpcGetIsOled(bool* out_is_oled)
{
    return serviceDispatchOut(&g_sysclkSrv, IpcCmd_GetIsOled, *out_is_oled);
}

Result IpcGetBatteryChargingDisabledOverride(bool* out_is_true)
{
    return serviceDispatchOut(&g_sysclkSrv, IpcCmd_GetBatteryChargingDisabledOverride, *out_is_true);
}

Result IpcSetBatteryChargingDisabledOverride(bool toggle_true)
{
    return serviceDispatchIn(&g_sysclkSrv, IpcCmd_SetBatteryChargingDisabledOverride, toggle_true);
}

Result IpcGetCustomValue(uint32_t index, uint32_t* out_value)
{
    return serviceDispatchInOut(&g_sysclkSrv, IpcCmd_GetCustomValue, index, *out_value);
}

Result IpcGetPrismEnabled(bool* out_enabled)
{
    return serviceDispatchOut(&g_sysclkSrv, IpcCmd_GetPrismEnabled, *out_enabled);
}

Result IpcSetPrismEnabled(bool enabled)
{
    return serviceDispatchIn(&g_sysclkSrv, IpcCmd_SetPrismEnabled, enabled);
}

Result IpcGetPrismMode(SysClkPrismMode* out_mode)
{
    return serviceDispatchOut(&g_sysclkSrv, IpcCmd_GetPrismMode, *out_mode);
}

Result IpcSetPrismMode(SysClkPrismMode mode)
{
    return serviceDispatchIn(&g_sysclkSrv, IpcCmd_SetPrismMode, mode);
}

Result IpcGetMemTimings(Timing timing, uint32_t* out_value)
{
    return serviceDispatchInOut(&g_sysclkSrv, IpcCmd_GetMemTimings, timing, *out_value);
}

Result IpcSetMemTimings(Timing timing, uint32_t value)
{
    struct {
        Timing timing;
        uint32_t value;
    } in = { timing, value };
    return serviceDispatchIn(&g_sysclkSrv, IpcCmd_SetMemTimings, in);
}

Result IpcGetFuelGaugeInfo(FuelGaugeInfo* out_info)
{
    return serviceDispatchOut(&g_sysclkSrv, IpcCmd_GetFuelGaugeInfo, *out_info);
}

Result IpcApplyBatteryCapacity(void)
{
    return serviceDispatch(&g_sysclkSrv, IpcCmd_ApplyBatteryCapacity);
}