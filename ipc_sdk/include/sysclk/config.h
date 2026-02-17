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
#include <stddef.h>

const uint32_t CHARGING_CURRENT_MA_LIMIT = 3000;
const uint32_t CHARGING_VOLTAGE_MV_MIN = 4000;
const uint32_t CHARGING_VOLTAGE_MV_MAX = 4400;

typedef enum {
    ConfigValue_PollingIntervalMs = 0,
    ConfigValue_TempLogIntervalMs,
    ConfigValue_CsvWriteIntervalMs,
    ConfigValue_AutoCPUBoost,
    ConfigValue_SyncReverseNXMode,
    ConfigValue_AllowUnsafeFrequencies,
    ConfigValue_ChargingCurrentLimit,
    ConfigValue_ChargingVoltageLimit,
    ConfigValue_ChargingVoltageAuto,     // 0=manual, 1=auto (use default voltage based on model)
    ConfigValue_ChargingLimitPercentage,
    ConfigValue_TdpLimit,
    ConfigValue_GovernorExperimental,
    //ConfigValue_GovernorHandheldOnly,
    ConfigValue_AutoArbitrateMc,
    ConfigValue_EmcMagician,
    ConfigValue_EmcPriority,
    ConfigValue_AltBrightness,
    ConfigValue_GovernorVrrSmooth,
    ConfigValue_PrismEnabled,
    ConfigValue_PrismMode,
    ConfigValue_LogFlag,
    ConfigValue_FpsBadass,
    ConfigValue_ColorMode,
    ConfigValue_EmcSolver,
    ConfigValue_Watchdog,               // 0=disabled, 1=EMC transition guard, 2=persistent heartbeat
    ConfigValue_BatteryCapacityScale,   // 0=disabled, 80-220 = percentage of design capacity
    ConfigValue_BatteryCapacityAuto,    // 0=disabled, 1=enabled (auto adjust based on cycles)
    ConfigValue_HomeLedBrightness,      // 0=off, 1-10 = brightness multiplier (10 = 100%)
    // ConfigValue_OledGamma,              // DISABLED: 0=default (60Hz), 1-4 = gamma correction levels
    ConfigValue_MagicWandEnabled,       // 0=off, 1=on (vibration active)
    ConfigValue_MagicWandStrength,      // 0-10 = vibration intensity
    ConfigValue_MagicWandMode,          // 0=constant, 1-5=patterns (pulse, wave, escalate, heartbeat, random)
    ConfigValue_EnumMax
} ConfigValue;

typedef struct {
    uint64_t values[ConfigValue_EnumMax];
} ConfigValueList;

static inline const char* sysclkFormatConfigValue(ConfigValue val, bool pretty)
{
    switch(val)
    {
        case ConfigValue_PollingIntervalMs:
            return pretty ? "Polling Interval (ms)" : "poll_interval_ms";
        case ConfigValue_TempLogIntervalMs:
            return pretty ? "Temperature logging interval (ms)" : "temp_log_interval_ms";
        case ConfigValue_CsvWriteIntervalMs:
            return pretty ? "CSV write interval (ms)" : "csv_write_interval_ms";
        case ConfigValue_AutoCPUBoost:
            return pretty ? "Auto CPU Boost" : "auto_cpu_boost";
        case ConfigValue_SyncReverseNXMode:
            return pretty ? "Reverse NX Sync" : "sync_reversenx_mode";
        case ConfigValue_AllowUnsafeFrequencies:
            return pretty ? "Badass Frequences" : "allow_unsafe_freq"; // Badass FPS Sync
        case ConfigValue_ChargingCurrentLimit:
            return pretty ? "Charging Current Limit (mA)" : "charging_current";
        case ConfigValue_ChargingVoltageLimit:
            return pretty ? "Charging Voltage Limit (mV)" : "charging_voltage";
        case ConfigValue_ChargingVoltageAuto:
            return pretty ? "Charging Voltage Auto" : "charging_voltage_auto";
        case ConfigValue_ChargingLimitPercentage:
            return pretty ? "Charging Limit (%%)" : "charging_limit_perc";
        case ConfigValue_TdpLimit:
            return pretty ? "TDP Limit (W)" : "tdp_limit_w";
        case ConfigValue_GovernorExperimental:
            return pretty ? "Active Governors" : "governor_experimental";
        //case ConfigValue_GovernorHandheldOnly:
        //    return pretty ? "Governors Handheld Only" : "governor_handheld_only";
        case ConfigValue_AutoArbitrateMc:
            return pretty ? "Active MC Arbitration" : "auto_arb_mc";
        case ConfigValue_EmcMagician:
            return pretty ? "EMC Magician" : "emc_magician";
        case ConfigValue_EmcPriority:
            return pretty ? "Adaptive EMC Priority" : "emc_priority";
        case ConfigValue_AltBrightness:
            return pretty ? "DC Dimming" : "alt_brightness";
        case ConfigValue_GovernorVrrSmooth:
            return pretty ? "LCD Governor Mode" : "governor_vrr_smooth";
        case ConfigValue_PrismEnabled:
            return pretty ? "PRISM Enabled" : "prism_enabled";
        case ConfigValue_PrismMode:
            return pretty ? "PRISM Mode" : "prism_mode";
        case ConfigValue_FpsBadass:
            return pretty ? "FPS Badass" : "fps_badass";
        case ConfigValue_LogFlag:
            return pretty ? "∏ℓ∑Đ∀∑" : "log_flag"; // ∏ℓ∑Đ∀∑ Logs Collector
        case ConfigValue_ColorMode:
            return pretty ? "Display Color Mode" : "color_mode";
        case ConfigValue_EmcSolver:
            return pretty ? "EMC Solver" : "emc_solver";
        case ConfigValue_Watchdog:
            return pretty ? "Watchdog" : "watchdog";
        case ConfigValue_BatteryCapacityScale:
            return pretty ? "Battery Capacity Scale (%)" : "battery_capacity_scale";
        case ConfigValue_BatteryCapacityAuto:
            return pretty ? "Battery Capacity Auto" : "battery_capacity_auto";
        case ConfigValue_HomeLedBrightness:
            return pretty ? "HOME LED Brightness" : "home_led_brightness";
        // case ConfigValue_OledGamma:
        //     return pretty ? "OLED Gamma" : "oled_gamma";
        case ConfigValue_MagicWandEnabled:
            return pretty ? "Magic Wand" : "magic_wand_enabled";
        case ConfigValue_MagicWandStrength:
            return pretty ? "Magic Wand Strength" : "magic_wand_strength";
        case ConfigValue_MagicWandMode:
            return pretty ? "Magic Wand Mode" : "magic_wand_mode";
        default:
            return NULL;
    }
}

static inline uint64_t sysclkDefaultConfigValue(ConfigValue val)
{
    switch(val)
    {
        case ConfigValue_PollingIntervalMs:
            return 500ULL;
        case ConfigValue_TempLogIntervalMs:
        case ConfigValue_CsvWriteIntervalMs:
        case ConfigValue_AllowUnsafeFrequencies:
        case ConfigValue_GovernorExperimental:
        //case ConfigValue_GovernorHandheldOnly:
        case ConfigValue_AutoCPUBoost:
        case ConfigValue_PrismEnabled:
        case ConfigValue_PrismMode:
        case ConfigValue_LogFlag:
        case ConfigValue_AltBrightness:
        case ConfigValue_ColorMode:
            return 0ULL;
        case ConfigValue_EmcSolver:
        case ConfigValue_Watchdog:
        case ConfigValue_FpsBadass:
        case ConfigValue_AutoArbitrateMc:
        case ConfigValue_EmcMagician:
		case ConfigValue_EmcPriority:
        case ConfigValue_SyncReverseNXMode:
            return 1ULL;
        case ConfigValue_ChargingCurrentLimit:
            return 2000ULL;
        case ConfigValue_ChargingVoltageLimit:
            return 4208ULL;
        case ConfigValue_ChargingVoltageAuto:
            return 1ULL;   // Auto mode enabled by default
        case ConfigValue_ChargingLimitPercentage:
            return 100ULL;
        case ConfigValue_TdpLimit:
            return 15ULL;
        case ConfigValue_GovernorVrrSmooth:
            return 2ULL;
        case ConfigValue_BatteryCapacityScale:
            return 0ULL;  // Disabled by default
        case ConfigValue_BatteryCapacityAuto:
            return 0ULL;  // Disabled by default
        case ConfigValue_HomeLedBrightness:
            return 10ULL;  // 100% brightness by default
        // case ConfigValue_OledGamma:
        //     return 1ULL;  // Default gamma level 1 (60Hz equivalent, neutral)
        case ConfigValue_MagicWandEnabled:
            return 0ULL;  // Disabled by default
        case ConfigValue_MagicWandStrength:
            return 5ULL;  // Medium strength by default
        case ConfigValue_MagicWandMode:
            return 0ULL;  // Constant mode by default
        default:
            return 0ULL;
    }
}

static inline uint64_t sysclkValidConfigValue(ConfigValue val, uint64_t input)
{
    switch(val)
    {
        case ConfigValue_PollingIntervalMs:
            return input > 0;
        case ConfigValue_TempLogIntervalMs:
        case ConfigValue_CsvWriteIntervalMs:
            return true;
        case ConfigValue_AutoCPUBoost:
        case ConfigValue_SyncReverseNXMode:
        case ConfigValue_AllowUnsafeFrequencies:
        //case ConfigValue_GovernorHandheldOnly:
        case ConfigValue_AutoArbitrateMc:
        case ConfigValue_EmcMagician:
		case ConfigValue_EmcPriority:
        case ConfigValue_AltBrightness:
        case ConfigValue_LogFlag:
        case ConfigValue_FpsBadass:
        case ConfigValue_PrismEnabled:
        case ConfigValue_EmcSolver:
            return (input & 0x1) == input;
        case ConfigValue_Watchdog:
            return input <= 2;
        case ConfigValue_GovernorExperimental:
            return (input & 7) == input; // 7 - это значение GovernorConfig_Mask (биты 0,1,2)
        case ConfigValue_ChargingCurrentLimit:
            return (input >= 100 && input <= CHARGING_CURRENT_MA_LIMIT && input % 100 == 0);
        case ConfigValue_ChargingVoltageLimit:
            return (input >= CHARGING_VOLTAGE_MV_MIN && input <= CHARGING_VOLTAGE_MV_MAX && input % 16 == 0);
        case ConfigValue_ChargingVoltageAuto:
            return (input & 0x1) == input;  // Boolean 0 or 1
        case ConfigValue_ChargingLimitPercentage:
            return (input <= 100 && input >= 20);
        case ConfigValue_TdpLimit:
            return (input >= 3 && input <= 16);
        case ConfigValue_PrismMode:
            return input <= 3;
        case ConfigValue_GovernorVrrSmooth:
            return (input >= 1 && input <= 3);
        case ConfigValue_ColorMode:
            return (input >= 0 && input <= 5);
        case ConfigValue_BatteryCapacityScale:
            // 0 = disabled, 80-220 = valid percentage range
            return (input == 0 || (input >= 80 && input <= 220));
        case ConfigValue_BatteryCapacityAuto:
            return (input & 0x1) == input;  // Boolean 0 or 1
        case ConfigValue_HomeLedBrightness:
            return (input <= 10);  // 0-10: 0=off, 1-10 = brightness multiplier (10% steps)
        // case ConfigValue_OledGamma:
        //     return (input <= 4);  // 0-4: 0=default, 1-4 = gamma correction levels
        case ConfigValue_MagicWandEnabled:
            return (input & 0x1) == input;  // Boolean 0 or 1
        case ConfigValue_MagicWandStrength:
            return (input <= 10);  // 0-10
        case ConfigValue_MagicWandMode:
            return (input <= 5);  // 0-5: constant, pulse, wave, escalate, heartbeat, random
        default:
            return false;
    }
}