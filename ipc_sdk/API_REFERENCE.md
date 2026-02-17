# 4IFIR IPC API Reference

Per-command documentation for all 37 IPC commands exposed by the 4IFIR sysmodule (API version 4).

See [README.md](README.md) for integration guide, data structure definitions, and usage examples.

---

## Table of Contents

1. [Connection & Version](#1-connection--version) (commands 0-1)
2. [System State](#2-system-state) (command 2)
3. [Lifecycle](#3-lifecycle) (command 3)
4. [Profile Management](#4-profile-management) (commands 4-6)
5. [Runtime Control](#5-runtime-control) (commands 7-8, 11)
6. [Configuration](#6-configuration) (commands 9-10)
7. [Hardware Query](#7-hardware-query) (commands 12-13, 16-17)
8. [Display](#8-display) (commands 18-21)
9. [Memory Timings](#9-memory-timings) (commands 22-23)
10. [Battery & Charging](#10-battery--charging) (commands 14-15, 35-36)
11. [SaltyNX Integration](#11-saltynx-integration) (commands 24-30)
12. [NxFps Integration](#12-nxfps-integration) (commands 31-34)

---

## 1. Connection & Version

### IpcCmd_GetApiVersion (0)

Returns the IPC protocol version. **Must be called first** after `IpcInitialize()` to verify compatibility.

**Signature:**
```c
Result IpcGetAPIVersion(u32* out_ver);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| Out | `u32` | API version number (currently `4`) |

**Transfer:** Inline (`serviceDispatchOut`)

**Safety:** Safe (read-only)

**Error conditions:** None (always succeeds if the service is connected).

**Example:**
```c
u32 ver;
Result rc = IpcGetAPIVersion(&ver);
if (R_FAILED(rc) || ver != API_VERSION) {
    IpcExit();
    return;
}
```

**Notes:** The version returned by the sysmodule must exactly match `API_VERSION` defined in `ipc.h`. There is no backward or forward compatibility.

---

### IpcCmd_GetVersionString (1)

Returns the sysmodule's human-readable version string (e.g. `"2.0.0"`).

**Signature:**
```c
Result IpcGetVersionString(char* out, size_t len);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| Out | `char[]` (buffer) | Null-terminated version string |

**Transfer:** HipcMapAlias buffer (Out)

**Safety:** Safe (read-only)

**Error conditions:** None. If `len == 0`, the buffer is not touched.

**Example:**
```c
char version[32];
Result rc = IpcGetVersionString(version, sizeof(version));
if (R_SUCCEEDED(rc)) {
    printf("4IFIR version: %s\n", version);
}
```

**Notes:** The string is the `TARGET_VERSION` compile-time constant from the sysmodule build.

---

## 2. System State

### IpcCmd_GetCurrentContext (2)

Returns a snapshot of the entire system state: frequencies, temperatures, voltages, power draw, RAM usage, active profile, and override status.

**Signature:**
```c
Result IpcGetCurrentContext(SysClkContext* out_context);
```

**Parameters:**
| Direction | Type | Size | Description |
|-----------|------|------|-------------|
| Out | `SysClkContext` (buffer) | ~120 bytes | Full system telemetry snapshot |

**Transfer:** HipcMapAlias buffer (Out)

**Safety:** Safe (read-only)

**Error conditions:** None.

**Side effects:** None. This is a pure read from the ClockManager's cached context.

**Example:**
```c
SysClkContext ctx;
if (R_SUCCEEDED(IpcGetCurrentContext(&ctx))) {
    printf("CPU: %u MHz @ %u mV, SOC: %.1f C\n",
        ctx.freqs[Module_CPU] / 1000000,
        ctx.voltages[Voltage_CPU],
        ctx.temps[SysClkThermalSensor_SOC] / 1000.0);
}
```

**Notes:**
- `freqs[]` = what 4IFIR requested; `realFreqs[]` = what hardware reports. They may differ during clock transitions.
- `overrideFreqs[i] == 0` means no override is active for module `i`.
- Temperatures are in **millidegrees Celsius** (42000 = 42.0 C).
- Power is in **milliwatts**. Voltages in **millivolts**.
- Recommended polling rate: 1-2 Hz. The sysmodule updates its context at its own tick rate (default 500ms).

---

## 3. Lifecycle

### IpcCmd_Exit (3)

**DANGEROUS.** Shuts down the entire 4IFIR sysmodule. All frequency management, governors, watchdog feeding, display control, and memory timing management stops immediately.

**Signature:**
```c
Result IpcExitCmd();
```

**Parameters:** None.

**Transfer:** Inline (`serviceDispatch`)

**Safety:** **DANGEROUS** -- stops the sysmodule system-wide.

**Error conditions:** None.

**Side effects:**
- The sysmodule's main loop exits.
- All frequency overrides are lost.
- Watchdog timer (if armed) will no longer be fed, potentially causing a reboot within 2 seconds if mode 2 was active.
- No more IPC commands will be accepted until the sysmodule is restarted.

**Example:**
```c
// DO NOT call this unless you are the 4IFIR overlay or a system management tool
Result rc = IpcExitCmd();
```

**Notes:** Only the 4IFIR overlay should use this command. Third-party homebrew should **never** call this.

---

## 4. Profile Management

### IpcCmd_GetProfileCount (4)

Returns the number of non-zero frequency slots configured for a given title ID.

**Signature:**
```c
Result IpcGetProfileCount(u64 tid, u8* out_count);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `u64` | Title ID (application ID) |
| Out | `u8` | Number of configured frequency slots |

**Transfer:** Inline (`serviceDispatchInOut`)

**Safety:** Safe (read-only)

**Error conditions:**
- `SysClkError_ConfigNotLoaded` if config hasn't been loaded yet.

**Example:**
```c
u8 count;
Result rc = IpcGetProfileCount(0x01004AB00A260000, &count);
if (R_SUCCEEDED(rc)) {
    printf("This game has %u configured frequency slots\n", count);
}
```

---

### IpcCmd_GetProfiles (5)

Returns the full profile list for a given title ID: frequencies (in MHz) for each profile-module combination, plus governor configuration.

**Signature:**
```c
Result IpcGetProfiles(u64 tid, SysClkTitleProfileList* out_profiles);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `u64` | Title ID. Use `GLOBAL_PROFILE_TID` (0xA111111111111111) for global defaults. |
| Out | `SysClkTitleProfileList` | Profile frequency map + governor config |

**Transfer:** Inline (`serviceDispatchInOut`)

**Safety:** Safe (read-only)

**Error conditions:**
- `SysClkError_ConfigNotLoaded` if config hasn't been loaded yet.

**Example:**
```c
SysClkTitleProfileList profiles;
Result rc = IpcGetProfiles(0x01004AB00A260000, &profiles);
if (R_SUCCEEDED(rc)) {
    uint32_t docked_cpu = profiles.mhzMap[SysClkProfile_Docked][Module_CPU];
    printf("Docked CPU: %u MHz\n", docked_cpu);
}
```

**Notes:** Frequency values are in **MHz** (not Hz). A value of 0 means "use default / not configured".

---

### IpcCmd_SetProfiles (6)

Saves the profile list for a given title ID. Writes to SD card config file.

**Signature:**
```c
Result IpcSetProfiles(u64 tid, SysClkTitleProfileList* profiles);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `u64 + SysClkTitleProfileList` | Title ID and the profile list to save |

**Transfer:** Inline (`serviceDispatchIn`, packed as `Ipc_SetProfiles_Args`)

**Safety:** Moderate (writes to SD card config)

**Error conditions:**
- `SysClkError_ConfigNotLoaded` if config hasn't been loaded yet.
- `SysClkError_ConfigSaveFailed` if SD card write fails.

**Side effects:** Writes to `/config/4IFIR/config.ini` on the SD card. Changes persist across reboots.

**Example:**
```c
SysClkTitleProfileList profiles;
IpcGetProfiles(tid, &profiles);
profiles.mhzMap[SysClkProfile_Docked][Module_CPU] = 1785;
IpcSetProfiles(tid, &profiles);
```

**Notes:** Always read-modify-write. Don't construct a profile list from scratch or you'll zero out existing settings for other profile+module slots.

---

## 5. Runtime Control

### IpcCmd_SetEnabled (7)

Enables or disables 4IFIR's dynamic clock management. When disabled, automatic profile-based frequency changes stop, but manual overrides (command 8) still work.

**Signature:**
```c
Result IpcSetEnabled(bool enabled);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `bool` (as `u8`) | `true` = enable, `false` = disable |

**Transfer:** Inline (`serviceDispatchIn`)

**Safety:** Moderate (changes runtime behavior)

**Error conditions:** None.

**Side effects:** When disabled, 4IFIR stops applying profile-based frequency targets. The system reverts to HOS default frequency management for any module without an active override.

**Example:**
```c
// Temporarily disable automatic management
IpcSetEnabled(false);
// ... do your own frequency control via overrides ...
// Re-enable
IpcSetEnabled(true);
```

---

### IpcCmd_SetOverride (8)

Temporarily overrides the target frequency for a hardware module. The override takes effect on the next tick. Pass `hz = 0` to remove the override.

**Signature:**
```c
Result IpcSetOverride(Module module, u32 hz);

// Convenience wrapper:
static inline Result IpcRemoveOverride(Module module) {
    return IpcSetOverride(module, 0);
}
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `Module` | Which module to override (CPU, GPU, MEM, LCD) |
| In | `u32` | Target frequency in Hz (0 = remove override) |

**Transfer:** Inline (`serviceDispatchIn`, packed as `Ipc_SetOverride_Args`)

**Safety:** Moderate (changes hardware clock frequency)

**Error conditions:**
- `SysClkError_Generic` if `module` is out of range.

**Side effects:**
- The sysmodule will request this frequency from the PCV service on the next tick.
- Override persists until explicitly removed or the sysmodule restarts.
- **Does not persist to config file.** Overrides are runtime-only.

**Example:**
```c
// Lock CPU to 1785 MHz
IpcSetOverride(Module_CPU, 1785000000);

// Remove when done
IpcRemoveOverride(Module_CPU);
```

**Notes:**
- The actual frequency achieved may differ from the requested frequency. Check `realFreqs[]` in the context.
- Use `IpcGetFrequencyTable` to discover valid frequencies.
- **Always remove overrides on app exit.** Orphaned overrides persist until sysmodule restart.

---

### IpcCmd_SetReverseNXRTMode (11)

Sets the ReverseNX runtime mode, forcing games to render in docked or handheld mode regardless of actual dock state.

**Signature:**
```c
Result IpcSetReverseNXRTMode(ReverseNXMode mode);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `ReverseNXMode` | `SystemDefault` (0), `Handheld` (1), or `Docked` (2) |

**Transfer:** Inline (`serviceDispatchIn`)

**Safety:** Moderate (changes game rendering mode)

**Error conditions:** None.

**Side effects:** The next time 4IFIR applies profiles, the ReverseNX mode will be factored in. Some games may need to be restarted for the change to take effect.

**Example:**
```c
// Force docked rendering while handheld (higher resolution)
IpcSetReverseNXRTMode(ReverseNX_Docked);

// Restore system default
IpcSetReverseNXRTMode(ReverseNX_SystemDefault);
```

---

## 6. Configuration

### IpcCmd_GetConfigValues (9)

Reads all 30 configuration values at once.

**Signature:**
```c
Result IpcGetConfigValues(ConfigValueList* out_configValues);
```

**Parameters:**
| Direction | Type | Size | Description |
|-----------|------|------|-------------|
| Out | `ConfigValueList` (buffer) | 240 bytes | All 30 config values as `uint64_t` array |

**Transfer:** HipcMapAlias buffer (Out)

**Safety:** Safe (read-only)

**Error conditions:**
- `SysClkError_ConfigNotLoaded` if config hasn't been loaded yet.

**Example:**
```c
ConfigValueList cfg;
Result rc = IpcGetConfigValues(&cfg);
if (R_SUCCEEDED(rc)) {
    uint64_t tdp = cfg.values[ConfigValue_TdpLimit];
    bool watchdog = cfg.values[ConfigValue_Watchdog] > 0;
    printf("TDP: %llu W, Watchdog: %s\n", tdp, watchdog ? "on" : "off");
}
```

**Notes:** See the ConfigValue enum in [README.md](README.md#configvalue) for the full list of indices, defaults, and valid ranges.

---

### IpcCmd_SetConfigValues (10)

Writes all 30 configuration values at once. Persists to SD card.

**Signature:**
```c
Result IpcSetConfigValues(ConfigValueList* configValues);
```

**Parameters:**
| Direction | Type | Size | Description |
|-----------|------|------|-------------|
| In | `ConfigValueList` (buffer) | 240 bytes | All 30 config values |

**Transfer:** HipcMapAlias buffer (In)

**Safety:** Moderate (writes to SD card, changes runtime behavior)

**Error conditions:**
- `SysClkError_ConfigNotLoaded` if config hasn't been loaded yet.
- `SysClkError_ConfigSaveFailed` if SD card write fails.

**Side effects:**
- Writes to `/config/4IFIR/config.ini` on SD card.
- Changes take effect immediately and persist across reboots.
- Invalid values for individual config entries are rejected by the sysmodule's validation logic (`sysclkValidConfigValue`).

**Example:**
```c
// Read-modify-write pattern (ALWAYS do this)
ConfigValueList cfg;
IpcGetConfigValues(&cfg);
cfg.values[ConfigValue_ChargingLimitPercentage] = 80;
cfg.values[ConfigValue_TdpLimit] = 12;
IpcSetConfigValues(&cfg);
```

**Notes:** Always use read-modify-write. Never construct a ConfigValueList from scratch, as you'd zero out all other settings.

---

## 7. Hardware Query

### IpcCmd_GetFrequencyTable (12)

Returns the list of available frequencies for a given module and profile combination.

**Signature:**
```c
Result IpcGetFrequencyTable(Module module, SysClkProfile profile, SysClkFrequencyTable* out_table);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `Module` | Which hardware module |
| In | `SysClkProfile` | Which power profile |
| Out | `SysClkFrequencyTable` | Array of available frequencies (Hz, zero-terminated) |

**Transfer:** Inline (`serviceDispatchInOut`, packed as `Ipc_GetFrequencyTable_Args` + output)

**Safety:** Safe (read-only)

**Error conditions:**
- `SysClkError_InternalFrequencyTableError` if the table cannot be retrieved.
- `SysClkError_Generic` if module/profile is out of range.

**Example:**
```c
SysClkFrequencyTable table;
Result rc = IpcGetFrequencyTable(Module_CPU, SysClkProfile_Docked, &table);
if (R_SUCCEEDED(rc)) {
    for (int i = 0; i < FREQ_TABLE_MAX_ENTRY_COUNT && table.freq[i] > 0; i++) {
        printf("  %u MHz\n", table.freq[i] / 1000000);
    }
}
```

**Notes:**
- Frequencies are in Hz, ascending order, zero-terminated.
- Available frequencies depend on loader patches (DVFS tables) and `AllowUnsafeFrequencies` config.
- The table may differ between profiles (e.g. docked mode may have higher GPU frequencies).

---

### IpcCmd_GetIsMariko (13)

Returns whether the console is a Mariko (V2/Lite/OLED) unit.

**Signature:**
```c
Result IpcGetIsMariko(bool* out_is_mariko);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| Out | `bool` | `true` = Mariko, `false` = Erista (V1) |

**Transfer:** Inline (`serviceDispatchOut`)

**Safety:** Safe (read-only, immutable hardware property)

**Error conditions:** None.

**Example:**
```c
bool mariko;
IpcGetIsMariko(&mariko);
if (mariko) {
    // Mariko has different voltage ranges and memory capabilities
}
```

---

### IpcCmd_GetCustomValue (16)

Reads an internal custom value by index. Currently returns the eBAMATIC state for any valid index.

**Signature:**
```c
Result IpcGetCustomValue(uint32_t index, uint32_t* out_value);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `u32` | Index (0-23) |
| Out | `u32` | Value |

**Transfer:** Inline (`serviceDispatchInOut`)

**Safety:** Safe (read-only)

**Error conditions:**
- `SysClkError_Generic` if `index >= 24`.

**Notes:** This is a debug/internal endpoint. The returned value is currently the eBAMATIC tuning state regardless of index. Subject to change.

---

### IpcCmd_GetIsOled (17)

Returns whether the console is an OLED model (Aula hardware type).

**Signature:**
```c
Result IpcGetIsOled(bool* out_is_oled);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| Out | `bool` | `true` = OLED Switch, `false` = LCD Switch |

**Transfer:** Inline (`serviceDispatchOut`)

**Safety:** Safe (read-only, immutable hardware property)

**Error conditions:** None.

**Example:**
```c
bool oled;
IpcGetIsOled(&oled);
if (oled) {
    // OLED-specific features (DC Dimming, PRISM) are available
}
```

**Notes:** An OLED Switch is always also Mariko, but not all Mariko units are OLED (Switch Lite, V2 are Mariko with LCD).

---

## 8. Display

### IpcCmd_GetPrismEnabled (18)

Returns whether PRISM display optimization is enabled.

**Signature:**
```c
Result IpcGetPrismEnabled(bool* out_enabled);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| Out | `bool` | `true` if PRISM is enabled |

**Transfer:** Inline (`serviceDispatchOut`)

**Safety:** Safe (read-only)

**Error conditions:** None.

---

### IpcCmd_SetPrismEnabled (19)

Enables or disables PRISM display optimization. Persists to config.

**Signature:**
```c
Result IpcSetPrismEnabled(bool enabled);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `bool` | `true` = enable, `false` = disable |

**Transfer:** Inline (`serviceDispatchIn`)

**Safety:** Moderate (writes to config, changes display behavior)

**Error conditions:**
- `SysClkError_ConfigSaveFailed` if SD card write fails.

**Side effects:** Updates `ConfigValue_PrismEnabled` and saves config to SD card.

**Example:**
```c
IpcSetPrismEnabled(true);
```

---

### IpcCmd_GetPrismMode (20)

Returns the current PRISM quality/battery stage.

**Signature:**
```c
Result IpcGetPrismMode(SysClkPrismMode* out_mode);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| Out | `SysClkPrismMode` | Current PRISM mode (0-5) |

**Transfer:** Inline (`serviceDispatchOut`)

**Safety:** Safe (read-only)

**Error conditions:** None.

---

### IpcCmd_SetPrismMode (21)

Sets the PRISM quality/battery stage. Persists to config.

**Signature:**
```c
Result IpcSetPrismMode(SysClkPrismMode mode);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `SysClkPrismMode` | PRISM mode (0=Off through 5=Stage3) |

**Transfer:** Inline (`serviceDispatchIn`)

**Safety:** Moderate (writes to config, changes display color processing)

**Error conditions:**
- `SysClkError_ConfigSaveFailed` if SD card write fails.

**Side effects:** Updates `ConfigValue_PrismMode` and saves config to SD card.

**Example:**
```c
// Set balanced mode
IpcSetPrismMode(SysClkPrismMode_Stage1);
```

**Notes:** PRISM modes affect display color and power consumption. Higher stages save more battery at the cost of color accuracy.

---

## 9. Memory Timings

### IpcCmd_GetMemTimings (22)

Reads a single memory timing parameter.

**Signature:**
```c
Result IpcGetMemTimings(Timing timing, uint32_t* out_value);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `Timing` | Timing parameter index (0 to `Timing_EnumMax - 1`) |
| Out | `u32` | Current timing value |

**Transfer:** Inline (`serviceDispatchInOut`)

**Safety:** Safe (read-only)

**Error conditions:**
- `SysClkError_Generic` if `timing` is out of range or `out_value` is NULL.

**Example:**
```c
uint32_t erp_val;
Result rc = IpcGetMemTimings(Timing_eRP, &erp_val);
if (R_SUCCEEDED(rc)) {
    printf("E-state RP timing: %u\n", erp_val);
}
```

**Notes:** See the Timing enum in [README.md](README.md#timing) for the full list of 54 timing parameters. E-state = low freq (<1612 MHz), S-state = high freq (>=1612 MHz).

---

### IpcCmd_SetMemTimings (23)

Writes a single memory timing parameter. **Changes are applied to hardware EMC registers.**

**Signature:**
```c
Result IpcSetMemTimings(Timing timing, uint32_t value);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `Timing` | Timing parameter index |
| In | `u32` | New timing value |

**Transfer:** Inline (`serviceDispatchIn`, packed as `{Timing, u32}`)

**Safety:** **Major** (directly modifies hardware memory controller registers)

**Error conditions:**
- `SysClkError_Generic` if `timing` is out of range.

**Side effects:**
- The timing value is queued and applied to EMC/MC registers on the next memory timing update cycle.
- **Incorrect timing values can cause memory corruption, system instability, or crashes.**
- Changes are runtime-only and do not persist across reboots.

**Example:**
```c
// Only if you know exactly what you're doing:
IpcSetMemTimings(Timing_eRP, 8);
```

**Notes:** This is an advanced/expert command. Incorrect memory timings can corrupt data in RAM. Use `IpcGetMemTimings` first to read current values and make only small adjustments.

---

## 10. Battery & Charging

### IpcCmd_GetBatteryChargingDisabledOverride (14)

Returns whether charging has been manually disabled via override.

**Signature:**
```c
Result IpcGetBatteryChargingDisabledOverride(bool* out_is_true);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| Out | `bool` | `true` if charging is disabled by override |

**Transfer:** Inline (`serviceDispatchOut`)

**Safety:** Safe (read-only)

**Error conditions:** None.

---

### IpcCmd_SetBatteryChargingDisabledOverride (15)

Enables or disables a charging override. When enabled, the battery will not charge even when connected to power.

**Signature:**
```c
Result IpcSetBatteryChargingDisabledOverride(bool toggle_true);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `bool` | `true` = disable charging, `false` = allow charging |

**Transfer:** Inline (`serviceDispatchIn`)

**Safety:** Moderate (controls charging hardware)

**Error conditions:** Returns result from the underlying charger control operation.

**Side effects:** Directly controls the battery charger IC. The override is runtime-only and resets on reboot.

**Example:**
```c
// Disable charging (e.g. for thermal management during intensive workloads)
IpcSetBatteryChargingDisabledOverride(true);

// Re-enable charging
IpcSetBatteryChargingDisabledOverride(false);
```

---

### IpcCmd_GetFuelGaugeInfo (35)

Returns battery health diagnostics from the MAX17050 fuel gauge IC.

**Signature:**
```c
Result IpcGetFuelGaugeInfo(FuelGaugeInfo* out_info);
```

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| Out | `FuelGaugeInfo` | Battery health data (20 bytes) |

**Transfer:** Inline (`serviceDispatchOut`)

**Safety:** Safe (read-only)

**Error conditions:** Returns result from the underlying fuel gauge read operation.

**Example:**
```c
FuelGaugeInfo info;
Result rc = IpcGetFuelGaugeInfo(&info);
if (R_SUCCEEDED(rc)) {
    printf("Capacity: %u / %u mAh (%u%% health)\n",
        info.fullCap, info.designCap, info.age);
    printf("Cycles: %u\n", info.cycles);
}
```

**Notes:**
- `fullCap`: Current full charge capacity in mAh (degrades over time).
- `designCap`: Factory design capacity in mAh.
- `fullCapNom`: Nominal full capacity (unfiltered estimate).
- `cycles`: Total charge cycle count.
- `age`: Health percentage = fullCap / designCap * 100.

---

### IpcCmd_ApplyBatteryCapacity (36)

Applies the `ConfigValue_BatteryCapacityScale` setting to the fuel gauge IC immediately, without waiting for the next automatic update cycle.

**Signature:**
```c
Result IpcApplyBatteryCapacity(void);
```

**Parameters:** None.

**Transfer:** Inline (`serviceDispatch`)

**Safety:** Moderate (writes to fuel gauge IC registers)

**Error conditions:** Returns result from the underlying fuel gauge write operation.

**Side effects:** Updates the MAX17050 fuel gauge's FullCap register based on the configured capacity scale percentage. This affects battery percentage reporting system-wide.

**Example:**
```c
// After changing battery capacity config:
ConfigValueList cfg;
IpcGetConfigValues(&cfg);
cfg.values[ConfigValue_BatteryCapacityScale] = 110;  // 110% of design
IpcSetConfigValues(&cfg);

// Apply immediately
IpcApplyBatteryCapacity();
```

---

## 11. SaltyNX Integration

Commands for SaltyNX plugin integration. Most are **deprecated** as docked refresh rate control has been moved to a direct implementation.

### IpcCmd_SaltyNX_GetConnected (24)

**Status: NOT IMPLEMENTED.** Declared in the enum but not dispatched by the service handler. Always returns `ERROR(Generic)`.

---

### IpcCmd_SaltyNX_SetDisplayRefreshRate (25)

**Status: DEPRECATED.** Docked refresh rate control has been moved to direct implementation in `docked_refresh_rate.cpp`. This command is commented out in the service handler and returns `ERROR(Generic)`.

---

### IpcCmd_SaltyNX_GetDisplayRefreshRate (26)

**Status: DEPRECATED.** See command 25.

---

### IpcCmd_SaltyNX_SetDisplaySync (27)

**Status: DEPRECATED.** See command 25.

---

### IpcCmd_SaltyNX_SetDockedRefreshRates (28)

**Status: DEPRECATED.** See command 25.

---

### IpcCmd_SaltyNX_SetDontForce60InDocked (29)

**Status: DEPRECATED.** See command 25.

---

### IpcCmd_SaltyNX_SetMatchLowestRR (30)

Controls render-interval matching to the lowest supported refresh rate. This is the only active SaltyNX command.

**Command ID:** 30

**Parameters:**
| Direction | Type | Description |
|-----------|------|-------------|
| In | `bool` | `true` = enable lowest-RR matching |

**Transfer:** Inline (`serviceDispatchIn`)

**Safety:** Moderate

**Error conditions:** Returns result from `SaltyNXManager::SetMatchLowestRR`.

**Notes:** No SDK client wrapper is provided. To call this directly, use libnx `serviceDispatchIn` with command ID 30. This command is primarily used by the SaltyNX plugin ecosystem.

---

## 12. NxFps Integration

Commands for NxFps shared-memory FPS counter integration. All are currently **declared but not dispatched** by the service handler.

### IpcCmd_NxFps_GetStatus (31)

**Status: NOT IMPLEMENTED.** Declared in the service header but not dispatched. Returns `ERROR(Generic)`.

Intended to return whether the NxFps shared-memory interface is active.

---

### IpcCmd_NxFps_SetFPSLocked (32)

**Status: NOT IMPLEMENTED.** Declared in the service header but not dispatched. Returns `ERROR(Generic)`.

Intended to lock the FPS to a specific target.

---

### IpcCmd_NxFps_SetZeroSync (33)

**Status: NOT IMPLEMENTED.** Declared in the service header but not dispatched. Returns `ERROR(Generic)`.

Intended to control zero-sync mode for frame timing.

---

### IpcCmd_NxFps_GetFPSInfo (34)

**Status: NOT IMPLEMENTED.** Declared in the service header but not dispatched. Returns `ERROR(Generic)`.

Intended to return current FPS data (current + average).

---

## Appendix: Command ID Summary

| ID | Enum Name | Active | SDK Wrapper |
|----|-----------|--------|-------------|
| 0 | `IpcCmd_GetApiVersion` | Yes | `IpcGetAPIVersion` |
| 1 | `IpcCmd_GetVersionString` | Yes | `IpcGetVersionString` |
| 2 | `IpcCmd_GetCurrentContext` | Yes | `IpcGetCurrentContext` |
| 3 | `IpcCmd_Exit` | Yes | `IpcExitCmd` |
| 4 | `IpcCmd_GetProfileCount` | Yes | `IpcGetProfileCount` |
| 5 | `IpcCmd_GetProfiles` | Yes | `IpcGetProfiles` |
| 6 | `IpcCmd_SetProfiles` | Yes | `IpcSetProfiles` |
| 7 | `IpcCmd_SetEnabled` | Yes | `IpcSetEnabled` |
| 8 | `IpcCmd_SetOverride` | Yes | `IpcSetOverride` / `IpcRemoveOverride` |
| 9 | `IpcCmd_GetConfigValues` | Yes | `IpcGetConfigValues` |
| 10 | `IpcCmd_SetConfigValues` | Yes | `IpcSetConfigValues` |
| 11 | `IpcCmd_SetReverseNXRTMode` | Yes | `IpcSetReverseNXRTMode` |
| 12 | `IpcCmd_GetFrequencyTable` | Yes | `IpcGetFrequencyTable` |
| 13 | `IpcCmd_GetIsMariko` | Yes | `IpcGetIsMariko` |
| 14 | `IpcCmd_GetBatteryChargingDisabledOverride` | Yes | `IpcGetBatteryChargingDisabledOverride` |
| 15 | `IpcCmd_SetBatteryChargingDisabledOverride` | Yes | `IpcSetBatteryChargingDisabledOverride` |
| 16 | `IpcCmd_GetCustomValue` | Yes | `IpcGetCustomValue` |
| 17 | `IpcCmd_GetIsOled` | Yes | `IpcGetIsOled` |
| 18 | `IpcCmd_GetPrismEnabled` | Yes | `IpcGetPrismEnabled` |
| 19 | `IpcCmd_SetPrismEnabled` | Yes | `IpcSetPrismEnabled` |
| 20 | `IpcCmd_GetPrismMode` | Yes | `IpcGetPrismMode` |
| 21 | `IpcCmd_SetPrismMode` | Yes | `IpcSetPrismMode` |
| 22 | `IpcCmd_GetMemTimings` | Yes | `IpcGetMemTimings` |
| 23 | `IpcCmd_SetMemTimings` | Yes | `IpcSetMemTimings` |
| 24 | `IpcCmd_SaltyNX_GetConnected` | No | -- |
| 25 | `IpcCmd_SaltyNX_SetDisplayRefreshRate` | No (deprecated) | -- |
| 26 | `IpcCmd_SaltyNX_GetDisplayRefreshRate` | No (deprecated) | -- |
| 27 | `IpcCmd_SaltyNX_SetDisplaySync` | No (deprecated) | -- |
| 28 | `IpcCmd_SaltyNX_SetDockedRefreshRates` | No (deprecated) | -- |
| 29 | `IpcCmd_SaltyNX_SetDontForce60InDocked` | No (deprecated) | -- |
| 30 | `IpcCmd_SaltyNX_SetMatchLowestRR` | Yes | -- |
| 31 | `IpcCmd_NxFps_GetStatus` | No | -- |
| 32 | `IpcCmd_NxFps_SetFPSLocked` | No | -- |
| 33 | `IpcCmd_NxFps_SetZeroSync` | No | -- |
| 34 | `IpcCmd_NxFps_GetFPSInfo` | No | -- |
| 35 | `IpcCmd_GetFuelGaugeInfo` | Yes | `IpcGetFuelGaugeInfo` |
| 36 | `IpcCmd_ApplyBatteryCapacity` | Yes | `IpcApplyBatteryCapacity` |
