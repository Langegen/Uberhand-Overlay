# 4IFIR IPC Client SDK

A standalone C client library for communicating with the **4IFIR sysmodule** on Nintendo Switch (Atmosphere CFW). Provides typed access to all IPC commands: telemetry, frequency control, configuration, display, memory timings, battery management, and more.

**Target audience**: Homebrew developers building applications that need to read Switch hardware telemetry or control overclocking/power management at runtime.

**Compatibility**: API version **4** (check with `IpcGetAPIVersion`). The SDK is protocol-stable: if the sysmodule reports a different API version, your app should gracefully degrade rather than crash.

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Integration](#integration)
3. [Data Structures](#data-structures)
4. [Enumerations](#enumerations)
5. [IPC Protocol](#ipc-protocol)
6. [Command Quick Reference](#command-quick-reference)
7. [Best Practices](#best-practices)
8. [Examples](#examples)
9. [File Map](#file-map)
10. [License](#license)

---

## Quick Start

### 1. Connect and verify

```c
#include <switch.h>
#include <sysclk/client/ipc.h>

// Check if the 4IFIR sysmodule is running
if (!IpcRunning()) {
    // Not installed or not running -- skip all 4IFIR integration
    return;
}

// Open the IPC session
Result rc = IpcInitialize();
if (R_FAILED(rc)) return;

// CRITICAL: verify wire-protocol compatibility
u32 apiVer;
rc = IpcGetAPIVersion(&apiVer);
if (R_FAILED(rc) || apiVer != API_VERSION) {
    IpcExit();
    return;  // version mismatch -- gracefully degrade
}
```

### 2. Read telemetry

```c
SysClkContext ctx;
rc = IpcGetCurrentContext(&ctx);
if (R_SUCCEEDED(rc)) {
    uint32_t cpu_hz   = ctx.freqs[Module_CPU];                   // e.g. 1785000000
    uint32_t gpu_hz   = ctx.freqs[Module_GPU];                   // e.g.  768000000
    uint32_t mem_hz   = ctx.freqs[Module_MEM];                   // e.g. 1600000000
    uint32_t soc_temp = ctx.temps[SysClkThermalSensor_SOC];      // millidegrees C (42000 = 42.0 C)
    uint32_t power_mw = ctx.power[SysClkPowerSensor_Now];        // milliwatts
    uint32_t cpu_mv   = ctx.voltages[Voltage_CPU];               // millivolts
}
```

### 3. Override frequencies

```c
// Lock CPU to 1785 MHz for a streaming session
rc = IpcSetOverride(Module_CPU, 1785000000);

// Lock GPU to 768 MHz
rc = IpcSetOverride(Module_GPU, 768000000);

// When done, ALWAYS remove overrides so 4IFIR resumes profile control
rc = IpcRemoveOverride(Module_CPU);   // equivalent to IpcSetOverride(Module_CPU, 0)
rc = IpcRemoveOverride(Module_GPU);
```

### 4. Disconnect

```c
IpcExit();
```

---

## Integration

### Makefile

```makefile
SDK_PATH := path/to/ipc_sdk

# Add to your existing INCLUDES and CFILES
INCLUDES += -I$(SDK_PATH)/include
CFILES   += $(SDK_PATH)/src/ipc.c
```

No additional libraries beyond **libnx** (`<switch.h>`) are needed.

### Include chain

```
your_code.c
  --> <sysclk/client/ipc.h>          Function declarations
       --> "types.h"                  libnx type shim (u32, Result, ...)
       --> "../config.h"              ConfigValue enum, ConfigValueList
       --> "../clocks.h"              Module, SysClkProfile, SysClkContext, Timing, ...
       --> "../ipc.h"                 IpcCmd enum, API_VERSION, arg structs, FuelGaugeInfo
```

All headers are self-contained. A single `#include <sysclk/client/ipc.h>` pulls in everything you need.

### C / C++ compatibility

All headers are plain C. The client header wraps declarations in `extern "C"` for C++ consumers. The `types.h` shim auto-detects `__SWITCH__` (defined by libnx) and falls back to `<stdint.h>` typedefs otherwise, allowing off-device compilation for testing.

---

## Data Structures

### SysClkContext

The primary telemetry structure returned by `IpcGetCurrentContext`. Transferred via HipcMapAlias buffer.

```c
typedef struct {
    uint8_t       enabled;                              // Is dynamic clock management active?
    uint64_t      applicationId;                        // Title ID of the running application
    SysClkProfile profile;                              // Current power profile
    uint32_t      freqs[Module_EnumMax];                // Target frequencies in Hz [CPU, GPU, MEM, LCD]
    uint32_t      realFreqs[Module_EnumMax];            // Actual HW frequencies in Hz
    uint32_t      overrideFreqs[Module_EnumMax];        // Active overrides (0 = no override)
    uint32_t      temps[SysClkThermalSensor_EnumMax];   // Temperatures in millidegrees C [SOC, PCB, Skin]
    uint32_t      perfConfId;                           // HOS performance configuration ID
    uint32_t      power[SysClkPowerSensor_EnumMax];     // Power draw in mW [Now, Avg]
    uint32_t      ramLoad[SysClkRamLoad_EnumMax];       // RAM utilization % [All, CPU]
    uint32_t      voltages[Voltage_EnumMax];            // Voltages in mV [SOC, VDD, VDQ, CPU, GPU]
} SysClkContext;
```

**Notes**:
- `freqs` vs `realFreqs`: `freqs` is what 4IFIR requested; `realFreqs` is what the hardware actually reports. They may differ during transitions or if a frequency was clamped.
- `overrideFreqs[i] == 0` means no override is active for that module.
- Temperature 42000 = 42.0 degrees C. Divide by 1000 for display.

### ConfigValueList

All 30 configuration values packed as an array of `uint64_t`. Transferred via HipcMapAlias buffer (240 bytes).

```c
typedef struct {
    uint64_t values[ConfigValue_EnumMax];   // ConfigValue_EnumMax = 30
} ConfigValueList;
```

Index with the `ConfigValue` enum:
```c
ConfigValueList cfg;
IpcGetConfigValues(&cfg);
uint64_t poll_ms = cfg.values[ConfigValue_PollingIntervalMs];  // default: 500
```

See [ConfigValue enum](#configvalue) for the full list of indices, defaults, and valid ranges.

### SysClkTitleProfileList

Per-application frequency profiles (one entry per profile-module combination) plus governor config.

```c
typedef struct {
    union {
        uint32_t mhz[(size_t)SysClkProfile_EnumMax * (size_t)Module_EnumMax];    // flat array
        uint32_t mhzMap[SysClkProfile_EnumMax][Module_EnumMax];                  // 2D access
    };
    GovernorConfig governorConfig;
} SysClkTitleProfileList;
```

Frequency values are in **MHz** (not Hz). A value of 0 means "use default / no override for this slot".

Access pattern:
```c
SysClkTitleProfileList profiles;
IpcGetProfiles(tid, &profiles);
uint32_t docked_cpu_mhz = profiles.mhzMap[SysClkProfile_Docked][Module_CPU];
```

The special title ID `GLOBAL_PROFILE_TID` (`0xA111111111111111`) accesses global (non-app-specific) defaults.

### SysClkFrequencyTable

List of available frequencies for a given module+profile combination.

```c
#define FREQ_TABLE_MAX_ENTRY_COUNT  31

typedef struct {
    uint32_t freq[FREQ_TABLE_MAX_ENTRY_COUNT];   // Hz, ascending, zero-terminated
} SysClkFrequencyTable;
```

Iterate until `freq[i] == 0`.

### FuelGaugeInfo

Battery health diagnostics from the MAX17050 fuel gauge IC.

```c
typedef struct {
    uint32_t fullCap;       // Current FullCap in mAh
    uint32_t designCap;     // Design capacity in mAh
    uint32_t fullCapNom;    // Nominal full capacity in mAh
    uint32_t cycles;        // Charge cycle count
    uint32_t age;           // Battery age % (FullCap / DesignCap * 100)
} FuelGaugeInfo;
```

### EmcContext

Memory timing values (54 parameters). Used internally by timing commands.

```c
typedef struct {
    uint32_t timings[Timing_EnumMax];   // Timing_EnumMax = 54
} EmcContext;
```

### IPC Argument Structs

```c
typedef struct { Module module; uint32_t hz; }           Ipc_SetOverride_Args;
typedef struct { uint64_t tid; SysClkTitleProfileList profiles; } Ipc_SetProfiles_Args;
typedef struct { Module module; SysClkProfile profile; } Ipc_GetFrequencyTable_Args;
```

---

## Enumerations

### Module

Hardware clock domains.

| Value | Name | Description |
|-------|------|-------------|
| 0 | `Module_CPU` | ARM Cortex-A57 quad-core cluster |
| 1 | `Module_GPU` | NVIDIA Maxwell GPU |
| 2 | `Module_MEM` | LPDDR4X memory controller (EMC) |
| 3 | `Module_LCD` | Display refresh rate |

### SysClkProfile

Power/charging profiles. Determines which frequency preset is active.

| Value | Name | Description |
|-------|------|-------------|
| 0 | `SysClkProfile_Handheld` | Undocked, not charging |
| 1 | `SysClkProfile_HandheldCharging` | Undocked, any charger |
| 2 | `SysClkProfile_HandheldChargingUSB` | Undocked, USB charger |
| 3 | `SysClkProfile_HandheldChargingOfficial` | Undocked, official PD charger |
| 4 | `SysClkProfile_Docked` | Docked mode |

### SysClkThermalSensor

Temperature sensor indices for `SysClkContext.temps[]`.

| Value | Name | Description |
|-------|------|-------------|
| 0 | `SysClkThermalSensor_SOC` | SoC (Tegra die) temperature |
| 1 | `SysClkThermalSensor_PCB` | PCB temperature |
| 2 | `SysClkThermalSensor_Skin` | Skin (external surface) temperature |

### SysClkPowerSensor

Power measurement indices for `SysClkContext.power[]`.

| Value | Name | Description |
|-------|------|-------------|
| 0 | `SysClkPowerSensor_Now` | Instantaneous power draw (mW) |
| 1 | `SysClkPowerSensor_Avg` | Average power draw (mW) |

### Voltage

Voltage rail indices for `SysClkContext.voltages[]`.

| Value | Name | Description |
|-------|------|-------------|
| 0 | `Voltage_SOC` | SoC core voltage |
| 1 | `Voltage_VDD` | Memory VDD2 voltage |
| 2 | `Voltage_VDQ` | Memory VDDQ voltage |
| 3 | `Voltage_CPU` | CPU voltage |
| 4 | `Voltage_GPU` | GPU voltage |

### SysClkRamLoad

RAM utilization indices for `SysClkContext.ramLoad[]`.

| Value | Name | Description |
|-------|------|-------------|
| 0 | `SysClkRamLoad_All` | Total RAM utilization % |
| 1 | `SysClkRamLoad_Cpu` | CPU-side RAM utilization % |

### Timing

Memory timing parameter indices (54 total). Used with `IpcGetMemTimings` / `IpcSetMemTimings`.

Timings are organized in groups of 4 for each parameter:
- `e*` prefix: E-state value (EMC freq < 1612 MHz)
- `s*` prefix: S-state value (EMC freq >= 1612 MHz)
- `ae*` prefix: MC ARB value for E-state
- `as*` prefix: MC ARB value for S-state

| Index Range | Parameter | Description |
|-------------|-----------|-------------|
| 0-3 | RP | Row Precharge time |
| 4-7 | RCD | RAS-to-CAS Delay |
| 8-11 | RC | Row Cycle time |
| 12-15 | RAS | Row Active Strobe |
| 16-19 | R2P | Read-to-Precharge delay |
| 20-23 | W2P | Write-to-Precharge delay |
| 24-27 | W2R | Write-to-Read turnaround |
| 28-31 | R2W | Read-to-Write turnaround |
| 32-35 | RFC | Refresh Cycle time |
| 36-39 | FAW | Four-Activate Window |
| 40-43 | RRD | Row-to-Row Delay |
| 44-47 | RCDW | RAS-to-CAS Delay for writes / MC ARB config |
| 48-50 | eVD2, eVDQ, eSOC | E-state voltage overrides (mV) |
| 51-53 | sVD2, sVDQ, sSOC | S-state voltage overrides (mV) |

### ReverseNXMode

Forced docked/handheld mode for game rendering.

| Value | Name | Description |
|-------|------|-------------|
| 0 | `ReverseNX_SystemDefault` | Follow system dock state |
| 1 | `ReverseNX_Handheld` | Force handheld rendering |
| 2 | `ReverseNX_Docked` | Force docked rendering |

### SysClkPrismMode

Display color/power optimization modes (PRISM = Panel self-Refresh and Idle Screen Management).

| Value | Name | Description |
|-------|------|-------------|
| 0 | `SysClkPrismMode_Off` | Disabled |
| 1 | `SysClkPrismMode_Reserved` | Reserved (highest quality) |
| 2 | `SysClkPrismMode_Default` | Default (higher quality) |
| 3 | `SysClkPrismMode_Stage1` | Balanced quality/battery |
| 4 | `SysClkPrismMode_Stage2` | Higher battery life |
| 5 | `SysClkPrismMode_Stage3` | Highest battery life |

### GovernorConfig

Bitmask controlling which hardware modules have automatic frequency scaling (governor).

| Value | Name | Description |
|-------|------|-------------|
| 0 | `GovernorConfig_AllDisabled` | No governors active |
| 1 | `GovernorConfig_CPU` | CPU governor enabled (bit 0) |
| 2 | `GovernorConfig_GPU` | GPU governor enabled (bit 1) |
| 4 | `GovernorConfig_LCD` | LCD governor enabled (bit 2) |
| 3 | `GovernorConfig_Default` | CPU + GPU governors |
| 7 | `GovernorConfig_AllEnabled` | All governors active |

Helper functions: `GetGovernorEnabled(config, module)`, `ToggleGovernor(prev, module, state)`.

### ConfigValue

Configuration parameter indices. Use with `ConfigValueList.values[]`.

| Index | Name | INI Key | Default | Valid Range | Description |
|-------|------|---------|---------|-------------|-------------|
| 0 | `PollingIntervalMs` | `poll_interval_ms` | 500 | > 0 | Main tick loop interval (ms) |
| 1 | `TempLogIntervalMs` | `temp_log_interval_ms` | 0 | any | Temperature logging interval (0 = off) |
| 2 | `CsvWriteIntervalMs` | `csv_write_interval_ms` | 0 | any | CSV telemetry export interval (0 = off) |
| 3 | `AutoCPUBoost` | `auto_cpu_boost` | 0 | 0-1 | Automatic CPU boost on load |
| 4 | `SyncReverseNXMode` | `sync_reversenx_mode` | 1 | 0-1 | Sync with ReverseNX tool |
| 5 | `AllowUnsafeFrequencies` | `allow_unsafe_freq` | 0 | 0-1 | Allow out-of-spec frequencies |
| 6 | `ChargingCurrentLimit` | `charging_current` | 2000 | 100-3000, step 100 | Charging current limit (mA) |
| 7 | `ChargingVoltageLimit` | `charging_voltage` | 4208 | 4000-4400, step 16 | Charging voltage limit (mV) |
| 8 | `ChargingVoltageAuto` | `charging_voltage_auto` | 1 | 0-1 | Auto voltage based on model |
| 9 | `ChargingLimitPercentage` | `charging_limit_perc` | 100 | 20-100 | Stop charging at this % |
| 10 | `TdpLimit` | `tdp_limit_w` | 15 | 3-16 | Total power budget (W) |
| 11 | `GovernorExperimental` | `governor_experimental` | 0 | 0-7 (bitmask) | Governor enable bitmask |
| 12 | `AutoArbitrateMc` | `auto_arb_mc` | 1 | 0-1 | Automatic MC arbitration |
| 13 | `EmcMagician` | `emc_magician` | 1 | 0-1 | EMC timing auto-tuner |
| 14 | `EmcPriority` | `emc_priority` | 1 | 0-1 | Adaptive EMC priority |
| 15 | `AltBrightness` | `alt_brightness` | 0 | 0-1 | DC Dimming (OLED) |
| 16 | `GovernorVrrSmooth` | `governor_vrr_smooth` | 2 | 1-3 | LCD governor mode |
| 17 | `PrismEnabled` | `prism_enabled` | 0 | 0-1 | PRISM display optimization |
| 18 | `PrismMode` | `prism_mode` | 0 | 0-3 | PRISM quality/battery stage |
| 19 | `LogFlag` | `log_flag` | 0 | 0-1 | Enable logging |
| 20 | `FpsBadass` | `fps_badass` | 0 | 0-1 | Aggressive FPS sync |
| 21 | `ColorMode` | `color_mode` | 0 | 0-5 | Display color mode preset |
| 22 | `EmcSolver` | `emc_solver` | 1 | 0-1 | EMC constraint solver |
| 23 | `Watchdog` | `watchdog` | 1 | 0-2 | PMIC watchdog mode (0=off, 1=EMC guard, 2=heartbeat) |
| 24 | `BatteryCapacityScale` | `battery_capacity_scale` | 0 | 0 or 80-220 | Battery capacity override % |
| 25 | `BatteryCapacityAuto` | `battery_capacity_auto` | 0 | 0-1 | Auto-adjust capacity by cycles |
| 26 | `HomeLedBrightness` | `home_led_brightness` | 10 | 0-10 | HOME button LED (0=off, 10=100%) |
| 27 | `MagicWandEnabled` | `magic_wand_enabled` | 0 | 0-1 | Haptic vibration feedback |
| 28 | `MagicWandStrength` | `magic_wand_strength` | 5 | 0-10 | Vibration intensity |
| 29 | `MagicWandMode` | `magic_wand_mode` | 0 | 0-5 | Vibration pattern |

---

## IPC Protocol

| Property | Value |
|----------|-------|
| Service name | `"4IFIR"` |
| API version | `4` (check with `IpcGetAPIVersion`) |
| Transport | HOS IPC via libnx `serviceDispatch*` family |
| Error module | `388` (error codes: `(388 & 0x1FF) | (desc << 9)`) |

### Buffer Transfer Modes

Most commands use **inline** transfer (`serviceDispatchIn` / `serviceDispatchOut` / `serviceDispatchInOut`), where data is embedded in the IPC message. The HOS inline limit is ~224 bytes.

Larger structures use **HipcMapAlias** buffer transfer:

| Command | Structure | Size | Direction |
|---------|-----------|------|-----------|
| `GetVersionString` (1) | `char[]` | variable | Out |
| `GetCurrentContext` (2) | `SysClkContext` | ~120 bytes | Out |
| `GetConfigValues` (9) | `ConfigValueList` | 240 bytes | Out |
| `SetConfigValues` (10) | `ConfigValueList` | 240 bytes | In |

### Error Codes

| Error | Value | Meaning |
|-------|-------|---------|
| `SysClkError_Generic` | 0 | Invalid arguments or unhandled command |
| `SysClkError_ConfigNotLoaded` | 1 | Config not yet loaded (called too early) |
| `SysClkError_ConfigSaveFailed` | 2 | Failed to write config to SD card |
| `SysClkError_InternalFrequencyTableError` | 3 | Frequency table lookup failed |

Decode: `R_MODULE(rc)` = 388, `R_DESCRIPTION(rc)` = error enum value.

---

## Command Quick Reference

All 37 commands (IDs 0-36). See [API_REFERENCE.md](API_REFERENCE.md) for detailed per-command documentation.

### Active Commands

| ID | Command | C Function | Direction | Safety |
|----|---------|------------|-----------|--------|
| 0 | `GetApiVersion` | `IpcGetAPIVersion` | -> `u32` | Safe |
| 1 | `GetVersionString` | `IpcGetVersionString` | -> `char[]` (buffer) | Safe |
| 2 | `GetCurrentContext` | `IpcGetCurrentContext` | -> `SysClkContext` (buffer) | Safe |
| 3 | `Exit` | `IpcExitCmd` | (none) | **DANGEROUS** |
| 4 | `GetProfileCount` | `IpcGetProfileCount` | `u64 tid` -> `u8` | Safe |
| 5 | `GetProfiles` | `IpcGetProfiles` | `u64 tid` -> `SysClkTitleProfileList` | Safe |
| 6 | `SetProfiles` | `IpcSetProfiles` | `{tid, profiles}` -> | Moderate |
| 7 | `SetEnabled` | `IpcSetEnabled` | `bool` -> | Moderate |
| 8 | `SetOverride` | `IpcSetOverride` | `{Module, u32 hz}` -> | Moderate |
| 9 | `GetConfigValues` | `IpcGetConfigValues` | -> `ConfigValueList` (buffer) | Safe |
| 10 | `SetConfigValues` | `IpcSetConfigValues` | `ConfigValueList` (buffer) -> | Moderate |
| 11 | `SetReverseNXRTMode` | `IpcSetReverseNXRTMode` | `ReverseNXMode` -> | Moderate |
| 12 | `GetFrequencyTable` | `IpcGetFrequencyTable` | `{Module, Profile}` -> `SysClkFrequencyTable` | Safe |
| 13 | `GetIsMariko` | `IpcGetIsMariko` | -> `bool` | Safe |
| 14 | `GetBatteryChargingDisabledOverride` | `IpcGetBatteryChargingDisabledOverride` | -> `bool` | Safe |
| 15 | `SetBatteryChargingDisabledOverride` | `IpcSetBatteryChargingDisabledOverride` | `bool` -> | Moderate |
| 16 | `GetCustomValue` | `IpcGetCustomValue` | `u32 index` -> `u32` | Safe |
| 17 | `GetIsOled` | `IpcGetIsOled` | -> `bool` | Safe |
| 18 | `GetPrismEnabled` | `IpcGetPrismEnabled` | -> `bool` | Safe |
| 19 | `SetPrismEnabled` | `IpcSetPrismEnabled` | `bool` -> | Moderate |
| 20 | `GetPrismMode` | `IpcGetPrismMode` | -> `SysClkPrismMode` | Safe |
| 21 | `SetPrismMode` | `IpcSetPrismMode` | `SysClkPrismMode` -> | Moderate |
| 22 | `GetMemTimings` | `IpcGetMemTimings` | `Timing` -> `u32` | Safe |
| 23 | `SetMemTimings` | `IpcSetMemTimings` | `{Timing, u32}` -> | **Major** |
| 30 | `SaltyNX_SetMatchLowestRR` | (no SDK wrapper) | `bool` -> | Moderate |
| 35 | `GetFuelGaugeInfo` | `IpcGetFuelGaugeInfo` | -> `FuelGaugeInfo` | Safe |
| 36 | `ApplyBatteryCapacity` | `IpcApplyBatteryCapacity` | (none) | Moderate |

**Safety levels**:
- **Safe**: Read-only, no side effects.
- **Moderate**: Changes runtime state. Reversible. May persist to SD card config.
- **Major**: Affects hardware registers directly. Can cause instability if misused.
- **DANGEROUS**: Shuts down the sysmodule. All overrides, governors, and management stop.

### Deprecated / Unimplemented Commands

| ID | Command | Status |
|----|---------|--------|
| 24 | `SaltyNX_GetConnected` | Declared but not dispatched; returns `ERROR(Generic)` |
| 25 | `SaltyNX_SetDisplayRefreshRate` | Deprecated: moved to direct implementation |
| 26 | `SaltyNX_GetDisplayRefreshRate` | Deprecated: moved to direct implementation |
| 27 | `SaltyNX_SetDisplaySync` | Deprecated: moved to direct implementation |
| 28 | `SaltyNX_SetDockedRefreshRates` | Deprecated: moved to direct implementation |
| 29 | `SaltyNX_SetDontForce60InDocked` | Deprecated: moved to direct implementation |
| 31 | `NxFps_GetStatus` | Declared but not dispatched; returns `ERROR(Generic)` |
| 32 | `NxFps_SetFPSLocked` | Declared but not dispatched; returns `ERROR(Generic)` |
| 33 | `NxFps_SetZeroSync` | Declared but not dispatched; returns `ERROR(Generic)` |
| 34 | `NxFps_GetFPSInfo` | Declared but not dispatched; returns `ERROR(Generic)` |

Do not use deprecated commands. They exist in the enum for backward compatibility but are not handled by the sysmodule.

---

## Best Practices

### Version Checking

Always check `IpcGetAPIVersion` immediately after `IpcInitialize`. If the version doesn't match `API_VERSION` from the SDK header, disconnect and disable your 4IFIR integration. Never assume forward or backward compatibility.

### Graceful Degradation

Design your app to work without 4IFIR:

```c
static bool g_4ifirAvailable = false;

void init4ifir() {
    if (!IpcRunning()) return;
    if (R_FAILED(IpcInitialize())) return;
    u32 ver;
    if (R_FAILED(IpcGetAPIVersion(&ver)) || ver != API_VERSION) {
        IpcExit();
        return;
    }
    g_4ifirAvailable = true;
}

void getTelemetry(MyTelemetry* out) {
    if (!g_4ifirAvailable) {
        // Fill with defaults or use libnx directly
        return;
    }
    SysClkContext ctx;
    if (R_SUCCEEDED(IpcGetCurrentContext(&ctx))) {
        out->cpuHz = ctx.freqs[Module_CPU];
        // ...
    }
}
```

### Thread Safety

The IPC client uses a **global** `Service` handle with a reference counter. The handle itself is not thread-safe. Options:

1. **Single-threaded**: Call all IPC functions from one thread (simplest).
2. **Mutex-wrapped**: Protect all IPC calls with a shared mutex.
3. **Init/Exit from main**: Call `IpcInitialize()` / `IpcExit()` from main thread only; dispatch IPC calls from one worker thread.

### Polling Rate

`IpcGetCurrentContext` is lightweight on the sysmodule side. Recommended polling rate: **1-2 Hz**. The sysmodule's own tick loop defaults to 500ms (`ConfigValue_PollingIntervalMs`), so polling faster than 2 Hz yields diminishing returns.

### Override Cleanup

If your app sets frequency overrides, **always** remove them on exit:

```c
void cleanup4ifir() {
    if (!g_4ifirAvailable) return;
    IpcRemoveOverride(Module_CPU);
    IpcRemoveOverride(Module_GPU);
    IpcRemoveOverride(Module_MEM);
    IpcExit();
    g_4ifirAvailable = false;
}
```

Register this as an `atexit()` handler or call it in your shutdown path. Orphaned overrides persist until the sysmodule restarts.

### Never Call Exit

`IpcExitCmd()` (command 3) **shuts down the entire 4IFIR sysmodule**. This stops all frequency management, governor logic, watchdog feeding, and display control system-wide. Only the 4IFIR overlay itself should use this command.

---

## Examples

### Telemetry Display

```c
#include <stdio.h>
#include <switch.h>
#include <sysclk/client/ipc.h>

void printTelemetry() {
    SysClkContext ctx;
    if (R_FAILED(IpcGetCurrentContext(&ctx))) return;

    printf("CPU: %u MHz (real: %u MHz)  Voltage: %u mV\n",
        ctx.freqs[Module_CPU] / 1000000,
        ctx.realFreqs[Module_CPU] / 1000000,
        ctx.voltages[Voltage_CPU]);

    printf("GPU: %u MHz  MEM: %u MHz\n",
        ctx.freqs[Module_GPU] / 1000000,
        ctx.freqs[Module_MEM] / 1000000);

    printf("SOC: %.1f C  PCB: %.1f C  Skin: %.1f C\n",
        ctx.temps[SysClkThermalSensor_SOC] / 1000.0,
        ctx.temps[SysClkThermalSensor_PCB] / 1000.0,
        ctx.temps[SysClkThermalSensor_Skin] / 1000.0);

    printf("Power: %u mW (avg: %u mW)\n",
        ctx.power[SysClkPowerSensor_Now],
        ctx.power[SysClkPowerSensor_Avg]);

    printf("Profile: %s  Enabled: %s\n",
        sysclkFormatProfile(ctx.profile, true),
        ctx.enabled ? "yes" : "no");
}
```

### Per-App Profile Management

```c
void setDockedCpuForGame(uint64_t tid, uint32_t cpu_mhz) {
    SysClkTitleProfileList profiles;
    Result rc = IpcGetProfiles(tid, &profiles);
    if (R_FAILED(rc)) return;

    profiles.mhzMap[SysClkProfile_Docked][Module_CPU] = cpu_mhz;

    rc = IpcSetProfiles(tid, &profiles);
    if (R_FAILED(rc)) {
        printf("Failed to save profile: 0x%x\n", rc);
    }
}
```

### Config Read-Modify-Write

```c
void setChargingLimit(uint32_t percent) {
    ConfigValueList cfg;
    Result rc = IpcGetConfigValues(&cfg);
    if (R_FAILED(rc)) return;

    cfg.values[ConfigValue_ChargingLimitPercentage] = percent;

    rc = IpcSetConfigValues(&cfg);
    if (R_FAILED(rc)) {
        printf("Failed to save config: 0x%x\n", rc);
    }
}
```

### Frequency Table Enumeration

```c
void listAvailableFreqs() {
    Module modules[] = { Module_CPU, Module_GPU, Module_MEM };
    const char* names[] = { "CPU", "GPU", "MEM" };

    for (int m = 0; m < 3; m++) {
        SysClkFrequencyTable table;
        Result rc = IpcGetFrequencyTable(modules[m], SysClkProfile_Docked, &table);
        if (R_FAILED(rc)) continue;

        printf("%s frequencies:\n", names[m]);
        for (int i = 0; i < FREQ_TABLE_MAX_ENTRY_COUNT && table.freq[i] > 0; i++) {
            printf("  %u MHz\n", table.freq[i] / 1000000);
        }
    }
}
```

### Battery Health Check

```c
void printBatteryHealth() {
    FuelGaugeInfo info;
    Result rc = IpcGetFuelGaugeInfo(&info);
    if (R_FAILED(rc)) return;

    printf("Battery: %u/%u mAh (%u%% health)\n",
        info.fullCap, info.designCap, info.age);
    printf("Cycles: %u  Nominal: %u mAh\n",
        info.cycles, info.fullCapNom);
}
```

### Hardware Detection

```c
void detectHardware() {
    bool mariko, oled;
    IpcGetIsMariko(&mariko);
    IpcGetIsOled(&oled);

    if (oled)       printf("OLED Switch (Aula)\n");
    else if (mariko) printf("Mariko Switch (V2 or Lite)\n");
    else            printf("Erista Switch (V1)\n");
}
```

---

## File Map

```
ipc_sdk/
  README.md                         This file -- overview, integration, reference
  API_REFERENCE.md                  Per-command detailed documentation

  include/sysclk/
    ipc.h                           IPC command enum (37 commands), API_VERSION,
                                    FuelGaugeInfo, argument structs
    clocks.h                        SysClkContext, Module, SysClkProfile, Timing,
                                    SysClkFrequencyTable, GovernorConfig,
                                    SysClkPrismMode, ReverseNXMode, format helpers
    config.h                        ConfigValue enum (30 values), ConfigValueList,
                                    default/validation functions
    errors.h                        Error module 388, error code enum

    client/
      ipc.h                         All client function declarations (C/C++ safe)
      types.h                       libnx type compatibility shim

  src/
    ipc.c                           Client function implementations (single file,
                                    ~215 lines, no dependencies beyond libnx)
```

---

## License

Beer-Ware License (Revision 42). Based on [sys-clk](https://github.com/retronx-team/sys-clk) by p-sam, natinusala, m4x.

> As long as you retain the license notice you can do whatever you want with this stuff. If you meet any of the authors some day, and you think this stuff is worth it, you can buy them a beer in return.
