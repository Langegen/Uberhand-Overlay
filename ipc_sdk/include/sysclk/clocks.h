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
#include <stdbool.h>

typedef enum
{
    SysClkProfile_Handheld = 0,
    SysClkProfile_HandheldCharging,
    SysClkProfile_HandheldChargingUSB,
    SysClkProfile_HandheldChargingOfficial,
    SysClkProfile_Docked,
    SysClkProfile_EnumMax
} SysClkProfile;

typedef enum
{
    Module_CPU = 0,
    Module_GPU,
    Module_MEM,
    Module_LCD,
    Module_EnumMax
} Module;

typedef enum
{
    SysClkThermalSensor_SOC = 0,
    SysClkThermalSensor_PCB,
    SysClkThermalSensor_Skin,
    SysClkThermalSensor_EnumMax
} SysClkThermalSensor;

typedef enum
{
    SysClkPowerSensor_Now = 0,
    SysClkPowerSensor_Avg,
    SysClkPowerSensor_EnumMax
} SysClkPowerSensor;

typedef enum
{
    Voltage_SOC = 0,
    Voltage_VDD,
    Voltage_VDQ,
    Voltage_CPU,
    Voltage_GPU,
    Voltage_EnumMax
} Voltage;

typedef enum
{
    SysClkRamLoad_All = 0,
    SysClkRamLoad_Cpu,
    SysClkRamLoad_EnumMax
} SysClkRamLoad;

typedef enum
{
    Timing_eRP = 0,     // RP для E состояния (EMC freq < 1612 mhz)
    Timing_sRP,        // RP для S состояния (EMC freq >= 1612 mhz)
    Timing_aeRP,      // MC ARB RP для E состояния (EMC freq < 1612 mhz)
    Timing_asRP,      // MC ARB RP для S состояния (EMC freq >= 1612 mhz)
    Timing_eRCD,     // RCD для E состояния (EMC freq < 1612 mhz)
    Timing_sRCD,     // RCD для S состояния (EMC freq >= 1612 mhz)
    Timing_aeRCD,    // MC ARB RCD для E состояния (EMC freq < 1612 mhz)
    Timing_asRCD,    // MC ARB RCD для S состояния (EMC freq >= 1612 mhz)
    Timing_eRC,      // RC для E состояния (EMC freq < 1612 mhz)
    Timing_sRC,      // RC для S состояния (EMC freq >= 1612 mhz)
    Timing_aeRC,     // MC ARB RC для E состояния (EMC freq < 1612 mhz)
    Timing_asRC,     // MC ARB RC для S состояния (EMC freq >= 1612 mhz)
    Timing_eRAS,     // RAS для E состояния (EMC freq < 1612 mhz)
    Timing_sRAS,     // RAS для S состояния (EMC freq >= 1612 mhz)
    Timing_aeRAS,    // MC ARB RAS для E состояния (EMC freq < 1612 mhz)
    Timing_asRAS,    // MC ARB RAS для S состояния (EMC freq >= 1612 mhz)
    Timing_eR2P,     // R2P для E состояния (EMC freq < 1612 mhz)
    Timing_sR2P,     // R2P для S состояния (EMC freq >= 1612 mhz)
    Timing_aeR2P,    // MC ARB R2P для E состояния (EMC freq < 1612 mhz)
    Timing_asR2P,    // MC ARB R2P для S состояния (EMC freq >= 1612 mhz)
    Timing_eW2P,     // W2P для E состояния (EMC freq < 1612 mhz)
    Timing_sW2P,     // W2P для S состояния (EMC freq >= 1612 mhz)
    Timing_aeW2P,    // MC ARB W2P для E состояния (EMC freq < 1612 mhz)
    Timing_asW2P,    // MC ARB W2P для S состояния (EMC freq >= 1612 mhz)
    Timing_eW2R,     // W2R для E состояния (EMC freq < 1612 mhz)
    Timing_sW2R,     // W2R для S состояния (EMC freq >= 1612 mhz)
    Timing_aeW2R,    // MC ARB W2R для E состояния (EMC freq < 1612 mhz)
    Timing_asW2R,    // MC ARB W2R для S состояния (EMC freq >= 1612 mhz)
    Timing_eR2W,     // R2W для E состояния (EMC freq < 1612 mhz)
    Timing_sR2W,     // R2W для S состояния (EMC freq >= 1612 mhz)
    Timing_aeR2W,    // MC ARB R2W для E состояния (EMC freq < 1612 mhz)
    Timing_asR2W,    // MC ARB R2W для S состояния (EMC freq >= 1612 mhz)
    Timing_eRFC,     // RFC для E состояния (EMC freq < 1612 mhz)
    Timing_sRFC,     // RFC для S состояния (EMC freq >= 1612 mhz)
    Timing_aeRFC,    // MC ARB RFC для E состояния (EMC freq < 1612 mhz) 
    Timing_asRFC,    // MC ARB RFC для S состояния (EMC freq >= 1612 mhz)
    Timing_eFAW,     // FAW для E состояния (EMC freq < 1612 mhz)
    Timing_sFAW,     // FAW для S состояния (EMC freq >= 1612 mhz)
    Timing_aeFAW,    // MC ARB FAW для E состояния (EMC freq < 1612 mhz)
    Timing_asFAW,    // MC ARB FAW для S состояния (EMC freq >= 1612 mhz)
    Timing_eRRD,     // RRD для E состояния (EMC freq < 1612 mhz)
    Timing_sRRD,     // RRD для S состояния (EMC freq >= 1612 mhz)
    Timing_aeRRD,    // MC ARB RRD для E состояния (EMC freq < 1612 mhz)
    Timing_asRRD,    // MC ARB RRD для S состояния (EMC freq >= 1612 mhz)
    Timing_eRCDW,     // RCDW для E состояния (EMC freq < 1612 mhz)
    Timing_sRCDW,     // RCDW для S состояния (EMC freq >= 1612 mhz)
    Timing_aeRCDW,    // MC ARB CFG для E состояния (EMC freq < 1612 mhz)
    Timing_asRCDW,    // MC ARB CFG для S состояния (EMC freq >= 1612 mhz)
	Timing_eVD2,
	Timing_eVDQ,
	Timing_eSOC,	
	Timing_sVD2,
	Timing_sVDQ,
	Timing_sSOC,
    Timing_EnumMax
} Timing;
/*
typedef enum
{
    A = 0,
    B,
    C,
    D,
    E,
    F
} CustTable;
*/
typedef struct {
    uint32_t timings[Timing_EnumMax];
} EmcContext;

typedef struct
{
    uint8_t  enabled;
    uint64_t applicationId;
    SysClkProfile profile;
    uint32_t freqs[Module_EnumMax];
    uint32_t realFreqs[Module_EnumMax];
    uint32_t overrideFreqs[Module_EnumMax];
    uint32_t temps[SysClkThermalSensor_EnumMax];
    uint32_t perfConfId;
	uint32_t power[SysClkPowerSensor_EnumMax];
	uint32_t ramLoad[SysClkRamLoad_EnumMax];
    uint32_t voltages[Voltage_EnumMax];
    //uint32_t timings[Timing_EnumMax]; // CAUTION !
} SysClkContext;

typedef enum
{
    ReverseNX_NotFound = 0,
    ReverseNX_SystemDefault = 0,
    ReverseNX_Handheld,
    ReverseNX_Docked,
} ReverseNXMode;

typedef struct
{
    bool systemCoreBoostCPU;
    bool batteryChargingDisabledOverride;
    SysClkProfile realProfile;
} Extra;

typedef enum {
    SysClkPrismMode_Off = 0,
    SysClkPrismMode_Reserved = 1,  // Highest quality
    SysClkPrismMode_Default = 2,  // Higher quality
    SysClkPrismMode_Stage1 = 3,   // Balanced
    SysClkPrismMode_Stage2 = 4,   // Higher battery life
    SysClkPrismMode_Stage3 = 5,   // Highest battery life
    SysClkPrismMode_EnumMax
} SysClkPrismMode;

#define FREQ_TABLE_MAX_ENTRY_COUNT  31

typedef struct
{
    uint32_t freq[FREQ_TABLE_MAX_ENTRY_COUNT];
} SysClkFrequencyTable;

typedef enum {
    GovernorConfig_AllDisabled  = 0,
    GovernorConfig_CPU_Shift    = 0,
    GovernorConfig_CPUOnly      = 1 << GovernorConfig_CPU_Shift,
    GovernorConfig_CPU          = 1 << GovernorConfig_CPU_Shift,
    GovernorConfig_GPU_Shift    = 1,
    GovernorConfig_GPUOnly      = 1 << GovernorConfig_GPU_Shift,
    GovernorConfig_GPU          = 1 << GovernorConfig_GPU_Shift,
    GovernorConfig_LCD_Shift    = 2,
    GovernorConfig_LCDOnly      = 1 << GovernorConfig_LCD_Shift,
    GovernorConfig_LCD          = 1 << GovernorConfig_LCD_Shift,
    GovernorConfig_AllEnabled   = 7,
    GovernorConfig_Default      = 3, // Changed from 7 to 3
    GovernorConfig_Mask         = 7,
} GovernorConfig;

inline bool GetGovernorEnabled(GovernorConfig config, Module module) {
    switch (module) {
        case Module_CPU:
            return (config >> GovernorConfig_CPU_Shift) & 1;
        case Module_GPU:
            return (config >> GovernorConfig_GPU_Shift) & 1;
        case Module_LCD:
            return (config >> GovernorConfig_LCD_Shift) & 1;
        case Module_MEM:
            return false;
        default:
            return config != GovernorConfig_AllDisabled;
    }
}

inline GovernorConfig ToggleGovernor(GovernorConfig prev, Module module, bool state) {
    uint8_t shift;
    switch (module) {
        case Module_CPU:
            shift = GovernorConfig_CPU_Shift;
            break;
        case Module_GPU:
            shift = GovernorConfig_GPU_Shift;
            break;
        case Module_LCD:
            shift = GovernorConfig_LCD_Shift;
            break;
        case Module_MEM:
            return prev;
        default:
            return state ? GovernorConfig_AllEnabled : GovernorConfig_AllDisabled;
    }
    return (GovernorConfig)((prev & ~(1 << shift)) | state << shift);
}

typedef struct
{
    union {
        uint32_t mhz[(size_t)SysClkProfile_EnumMax * (size_t)Module_EnumMax];
        uint32_t mhzMap[SysClkProfile_EnumMax][Module_EnumMax];
    };
    GovernorConfig governorConfig;
} SysClkTitleProfileList;

#define GLOBAL_PROFILE_TID       0xA111111111111111

extern uint32_t g_freq_table_mem_hz[];
extern uint32_t g_freq_table_cpu_hz[];
extern uint32_t g_freq_table_gpu_hz[];
extern uint32_t g_freq_table_lcd_hz[];

#define ENUM_VALID(n, v) ((v) < n##_EnumMax)

static inline const char* sysclkFormatModule(Module module, bool pretty)
{
    switch(module)
    {
        case Module_CPU:
            return pretty ? "CPU" : "cpu";
        case Module_GPU:
            return pretty ? "GPU" : "gpu";
        case Module_MEM:
            return pretty ? "EMC" : "mem";
        case Module_LCD:
            return pretty ? "LCD" : "lcd";
        default:
            return NULL;
    }
}

static inline const char* sysclkFormatThermalSensor(SysClkThermalSensor thermSensor, bool pretty)
{
    switch(thermSensor)
    {
        case SysClkThermalSensor_SOC:
            return pretty ? "SOC" : "soc";
        case SysClkThermalSensor_PCB:
            return pretty ? "PCB" : "pcb";
        case SysClkThermalSensor_Skin:
            return pretty ? "TSN" : "tsn";
        default:
            return NULL;
    }
}

static inline const char* sysclkFormatVoltage(Voltage voltage, bool pretty)
{
    switch(voltage)
    {
        case Voltage_SOC:
            return pretty ? "SOC" : "soc";
        case Voltage_VDD:
            return pretty ? "VDD" : "vdd";
        case Voltage_VDQ:
            return pretty ? "VDQ" : "vdq";
        case Voltage_CPU:
            return pretty ? "CPU" : "cpu";
        case Voltage_GPU:
            return pretty ? "GPU" : "gpu";
        default:
            return NULL;
    }
}

static inline const char* sysclkFormatProfile(SysClkProfile profile, bool pretty)
{
    switch(profile)
    {
        case SysClkProfile_Docked:
            return pretty ? "TELE4-mo" : "docked";
        case SysClkProfile_Handheld:
            return pretty ? "Handheld" : "handheld";
        case SysClkProfile_HandheldCharging:
            return pretty ? "Charging" : "handheld_charging";
        case SysClkProfile_HandheldChargingUSB:
            return pretty ? "USB Chrgr" : "handheld_charging_usb";
        case SysClkProfile_HandheldChargingOfficial:
            return pretty ? "OfficialPD" : "handheld_charging_official";
        default:
            return NULL;
    }
}

static inline const char* sysclkFormatPowerSensor(SysClkPowerSensor powSensor, bool pretty)
{
    switch(powSensor)
    {
        case SysClkPowerSensor_Now:
            return pretty ? "Now" : "now";
        case SysClkPowerSensor_Avg:
            return pretty ? "Avg" : "avg";
        default:
            return NULL;
    }
}

static inline const char* sysclkFormatPrismMode(SysClkPrismMode prismMode, bool pretty)
{
    switch(prismMode)
    {
        case SysClkPrismMode_Off:
            return pretty ? "Disabled" : "disabled";
        case SysClkPrismMode_Reserved:
            return pretty ? "Reserved" : "reserved";
        case SysClkPrismMode_Default:
            return pretty ? "Default" : "default";
        case SysClkPrismMode_Stage1:
            return pretty ? "ECO ST1" : "stage1";
        case SysClkPrismMode_Stage2:
            return pretty ? "ECO ST2" : "stage2";
        case SysClkPrismMode_Stage3:
            return pretty ? "ECO ST3" : "stage3";
        default:
            return NULL;
    }
}