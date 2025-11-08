#include <switch.h>
#include <string.h>
#include <stdlib.h>

#define CHEKATE_FILES_COUNT 4
#define CHEKATE_STAGES_COUNT 3
#define CHEKATE_UNKNOWN_STAGE "4IFIX - Active"

typedef struct {
    u32 mc_emem_adr_cfg_channel_mask;
    u32 mc_emem_adr_cfg_bank_mask0;
    u32 mc_emem_adr_cfg_bank_mask1;
    u32 mc_emem_adr_cfg_bank_mask2;
} CHEKATEParams;

extern FsFileSystem sdFs;
extern bool fsInitialized;
extern bool chekateStageWasChanged;

extern const char* chekateFilesPaths[CHEKATE_FILES_COUNT];
extern s64 paramsOffsets[CHEKATE_FILES_COUNT];
extern const CHEKATEParams stages[CHEKATE_STAGES_COUNT];
extern const char* stagesTitles[CHEKATE_STAGES_COUNT];
extern const char* chifixTitle;
extern const u8 chifixPattern[4];

bool compareU8Arrays(const u8* a, const u8* b, size_t size);

s64 searchBytesArray(FsFile* file, const u8* pattern, size_t patternSize, s64 startOffset) {
    s64 fileSize;
    Result rc = fsFileGetSize(file, &fileSize);
    if (R_FAILED(rc)) {
        return -1;
    }

    if (startOffset < 0 || startOffset > fileSize - (s64)patternSize) {
        return -1;
    }

    const size_t bufferSize = 4096;
    u8* buffer = (u8*)malloc(bufferSize);
    if (!buffer) {
        return -1;
    }

    s64 offset = startOffset;
    while (offset <= fileSize - (s64)patternSize) {
        size_t readSize = (size_t)(fileSize - offset) > bufferSize ? bufferSize : (size_t)(fileSize - offset);
        u64 bytesRead;
        rc = fsFileRead(file, offset, buffer, readSize, 0, &bytesRead);
        if (R_FAILED(rc) || bytesRead == 0) {
            free(buffer);
            return -1;
        }

        for (size_t i = 0; i <= bytesRead - patternSize; i++) {
            if (memcmp(buffer + i, pattern, patternSize) == 0) {
                free(buffer);
                return offset + i;
            }
        }
        offset += bytesRead - patternSize + 1;
    }
    free(buffer);
    return -1;
}

s64 getParamsOffset(const char* filePath, s64 start) {
    FsFile file;
    Result rc = fsFsOpenFile(&sdFs, filePath, FsOpenMode_Read, &file);
    if (R_FAILED(rc)) {
        return -1;
    }
    for (int i = 0; i < CHEKATE_STAGES_COUNT; ++i) {
        s64 offset = searchBytesArray(&file, (const u8*)(&stages[i]), sizeof(CHEKATEParams), start);
        if (offset != -1) {
            fsFileClose(&file);
            return offset;
        }
    }
    fsFileClose(&file);
    return -1;
}

void set4ekateStagesOffsets() {
    if (!fsInitialized) {
        Result rc = fsInitialize();
        if (R_SUCCEEDED(rc)) {
            rc = fsOpenSdCardFileSystem(&sdFs);
            if (R_SUCCEEDED(rc)) {
                fsInitialized = true;
            }
        }
    }
    for (int i = 0; i < CHEKATE_FILES_COUNT; ++i) {
        paramsOffsets[i] = getParamsOffset(chekateFilesPaths[i], 0x00015000);
    }
}

void load4EKATEParams(CHEKATEParams* params) {
    for (int i = 0; i < CHEKATE_FILES_COUNT; ++i) {
        if (paramsOffsets[i] == -1) {
            return;
        }
    }
    FsFile file;
    Result rc = fsFsOpenFile(&sdFs, chekateFilesPaths[0], FsOpenMode_Read, &file);
    if (R_FAILED(rc)) {
        return;
    }
    u64 bytesRead;
    rc = fsFileRead(&file, paramsOffsets[0], params, sizeof(CHEKATEParams), 0, &bytesRead);
    fsFileClose(&file);
    if (R_FAILED(rc)) {
        return;
    }
}

bool chekateFilesExists() {
    if (!fsInitialized) {
        return false;
    }
    for (int i = 0; i < CHEKATE_FILES_COUNT; ++i) {
        FsDirEntryType type;
        Result rc = fsFsGetEntryType(&sdFs, chekateFilesPaths[i], &type);
        if (R_FAILED(rc) || type != FsDirEntryType_File) {
            return false;
        }
    }
    return true;
}

bool is4IFIXPresent() {
    FsFile file;
    Result rc = fsFsOpenFile(&sdFs, chekateFilesPaths[0], FsOpenMode_Read, &file);
    if (R_FAILED(rc)) {
        return false;
    }

    s64 offset = searchBytesArray(&file, chifixPattern, sizeof(chifixPattern), 0);
    fsFileClose(&file);
    if (offset != -1) {
        return true;
    }
    return false;
}

bool patch4ekateStage(int stageId) {
    if (stageId < 0 || stageId >= CHEKATE_STAGES_COUNT) {
        return false;
    }

    if (is4IFIXPresent()) {
        return false;
    }

    if (!chekateFilesExists()) {
        return false;
    }

    for (int i = 0; i < CHEKATE_FILES_COUNT; ++i) {
        if (paramsOffsets[i] == -1) {
            return false;
        }
    }

    CHEKATEParams params = stages[stageId];
    for (int i = 0; i < CHEKATE_FILES_COUNT; ++i) {
        FsFile file;
        Result rc = fsFsOpenFile(&sdFs, chekateFilesPaths[i], FsOpenMode_Write, &file);
        if (R_FAILED(rc)) {
            return false;
        }
        rc = fsFileWrite(&file, paramsOffsets[i], &params, sizeof(CHEKATEParams), 0);
        fsFileClose(&file);
        if (R_FAILED(rc)) {
            return false;
        }
    }

    if (!chekateStageWasChanged) chekateStageWasChanged = true;
    return true;
}

int getCurrentStageId() {
    if (is4IFIXPresent()) {
        return -2;
    }

    for (int i = 0; i < CHEKATE_FILES_COUNT; ++i) {
        if (paramsOffsets[i] == -1) {
            return -1;
        }
    }

    CHEKATEParams* params = (CHEKATEParams*)malloc(sizeof(CHEKATEParams));
    if (!params) {
        return -1;
    }

    load4EKATEParams(params);
    for (int i = 0; i < CHEKATE_STAGES_COUNT; ++i) {
        if (compareU8Arrays((u8*)params, (u8*)&stages[i], sizeof(CHEKATEParams))) {
            free(params);
            return i;
        }
    }
    free(params);
    return -1;
}

const char* getCurrentStageTitle() {
    const int stageId = getCurrentStageId();
    if (stageId == -1) {
        return CHEKATE_UNKNOWN_STAGE;
    } else if (stageId == -2) {
        return chifixTitle;
    }
    return stagesTitles[stageId];
}

FsFileSystem sdFs;
bool fsInitialized = false;
bool chekateStageWasChanged = false;

const char* chekateFilesPaths[CHEKATE_FILES_COUNT] = {
    "/payload.bin",
    "/bootloader/payloads/fusee.bin",
    "/atmosphere/reboot_payload.bin",
    "/bootloader/update.bin"
};
s64 paramsOffsets[CHEKATE_FILES_COUNT] = {0, 0, 0, 0};

const CHEKATEParams stages[CHEKATE_STAGES_COUNT] = {
    {.mc_emem_adr_cfg_channel_mask = 0xFFFF2400, .mc_emem_adr_cfg_bank_mask0 = 0x6E574400, .mc_emem_adr_cfg_bank_mask1 = 0x39722800, .mc_emem_adr_cfg_bank_mask2 = 0x4B9C1000},
    {.mc_emem_adr_cfg_channel_mask = 0x00002400, .mc_emem_adr_cfg_bank_mask0 = 0x6E574400, .mc_emem_adr_cfg_bank_mask1 = 0x39722800, .mc_emem_adr_cfg_bank_mask2 = 0x4B9C1000},
    {.mc_emem_adr_cfg_channel_mask = 0x00002400, .mc_emem_adr_cfg_bank_mask0 = 0x00004400, .mc_emem_adr_cfg_bank_mask1 = 0x00002800, .mc_emem_adr_cfg_bank_mask2 = 0x00001000}
};

const char* stagesTitles[CHEKATE_STAGES_COUNT] = {"4EKATE Stage - Default", "4EKATE Stage - ST1", "4EKATE Stage - ST2"};
const char* chifixTitle = "4EKATE - 4IFIX";
const u8 chifixPattern[4] = {0xa6, 0x34, 0x02, 0x90};

bool compareU8Arrays(const u8* a, const u8* b, size_t size) {
    return memcmp(a, b, size) == 0;
}