#include "4ekate.hpp"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
const char* offsetsCachePath = "/config/Uberhand/4ekate_offsets.bin";

const CHEKATEParams stages[CHEKATE_STAGES_COUNT] = {
    {.mc_emem_adr_cfg_channel_mask = 0xFFFF2400, .mc_emem_adr_cfg_bank_mask0 = 0x6E574400, .mc_emem_adr_cfg_bank_mask1 = 0x39722800, .mc_emem_adr_cfg_bank_mask2 = 0x4B9C1000},
    {.mc_emem_adr_cfg_channel_mask = 0x00002400, .mc_emem_adr_cfg_bank_mask0 = 0x6E574400, .mc_emem_adr_cfg_bank_mask1 = 0x39722800, .mc_emem_adr_cfg_bank_mask2 = 0x4B9C1000},
    {.mc_emem_adr_cfg_channel_mask = 0x00002400, .mc_emem_adr_cfg_bank_mask0 = 0x00004400, .mc_emem_adr_cfg_bank_mask1 = 0x00002800, .mc_emem_adr_cfg_bank_mask2 = 0x00001000}
};

const char* stagesTitles[CHEKATE_STAGES_COUNT] = {"4EKATE Stage - Default", "4EKATE Stage - ST1", "4EKATE Stage - ST2"};
const char* chifixTitle = "4EKATE - 4IFIX";
const u8 chifixPattern[4] = {0x46, 0x38, 0x02, 0x40}; // Проверьте правильность сигнатуры

bool compareU8Arrays(const u8* a, const u8* b, size_t size) {
    return memcmp(a, b, size) == 0;
}

void loadOffsetsCache() {
    FsFile file;
    Result rc = fsFsOpenFile(&sdFs, offsetsCachePath, FsOpenMode_Read, &file);
    if (R_SUCCEEDED(rc)) {
        u64 bytesRead;
        rc = fsFileRead(&file, 0, paramsOffsets, sizeof(paramsOffsets), 0, &bytesRead);
        fsFileClose(&file);
        if (R_FAILED(rc) || bytesRead != sizeof(paramsOffsets)) {
            memset(paramsOffsets, 0, sizeof(paramsOffsets));
        } else {
            for (int i = 0; i < CHEKATE_FILES_COUNT; ++i) {
                if (paramsOffsets[i] < 0) {
                    memset(paramsOffsets, 0, sizeof(paramsOffsets));
                    break;
                }
            }
        }
    }
}

void saveOffsetsCache() {
    fsFsCreateDirectory(&sdFs, "/config");
    fsFsCreateDirectory(&sdFs, "/config/Uberhand");
    FsFile file;
    Result rc = fsFsCreateFile(&sdFs, offsetsCachePath, sizeof(paramsOffsets), 0);
    if (R_SUCCEEDED(rc)) {
        rc = fsFsOpenFile(&sdFs, offsetsCachePath, FsOpenMode_Write, &file);
        if (R_SUCCEEDED(rc)) {
            rc = fsFileWrite(&file, 0, paramsOffsets, sizeof(paramsOffsets), 0);
            fsFileClose(&file);
        }
    }
}

s64 searchBytesArray(FsFile* file, const u8* pattern, size_t patternSize, s64 startOffset) {
    s64 fileSize;
    Result rc = fsFileGetSize(file, &fileSize);
    if (R_FAILED(rc)) {
        printf("searchBytesArray: Failed to get file size: 0x%x\n", rc);
        return -1;
    }

    if (startOffset < 0 || startOffset > fileSize - (s64)patternSize) {
        printf("searchBytesArray: Invalid start offset: 0x%lx\n", startOffset);
        return -1;
    }

    const size_t bufferSize = 4096;
    u8* buffer = (u8*)malloc(bufferSize);
    if (!buffer) {
        printf("searchBytesArray: Memory allocation failed\n");
        return -1;
    }

    s64 offset = startOffset;
    while (offset <= fileSize - (s64)patternSize) {
        size_t readSize = (size_t)(fileSize - offset) > bufferSize ? bufferSize : (size_t)(fileSize - offset);
        u64 bytesRead;
        rc = fsFileRead(file, offset, buffer, readSize, 0, &bytesRead);
        if (R_FAILED(rc) || bytesRead == 0) {
            printf("searchBytesArray: Read failed at offset 0x%lx: 0x%x\n", offset, rc);
            free(buffer);
            return -1;
        }

        for (size_t i = 0; i <= bytesRead - patternSize; i++) {
            if (memcmp(buffer + i, pattern, patternSize) == 0) {
                printf("searchBytesArray: Pattern found at offset 0x%lx\n", offset + i);
                free(buffer);
                return offset + i;
            }
        }
        offset += bytesRead - patternSize + 1;
    }
    free(buffer);
    printf("searchBytesArray: Pattern not found\n");
    return -1;
}

s64 getParamsOffset(const char* filePath, s64 start) {
    FsFile file;
    Result rc = fsFsOpenFile(&sdFs, filePath, FsOpenMode_Read, &file);
    if (R_FAILED(rc)) {
        printf("getParamsOffset: Failed to open %s: 0x%x\n", filePath, rc);
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
            } else {
                printf("set4ekateStagesOffsets: Failed to open SD card: 0x%x\n", rc);
            }
        } else {
            printf("set4ekateStagesOffsets: Failed to initialize FS: 0x%x\n", rc);
        }
    }
    loadOffsetsCache();
    bool needSave = false;
    for (int i = 0; i < CHEKATE_FILES_COUNT; ++i) {
        if (paramsOffsets[i] == 0) {
            paramsOffsets[i] = getParamsOffset(chekateFilesPaths[i], 0x00015000);
            needSave = true;
        }
    }
    if (needSave) {
        saveOffsetsCache();
    }
}

void load4EKATEParams(CHEKATEParams* params) {
    for (int i = 0; i < CHEKATE_FILES_COUNT; ++i) {
        if (paramsOffsets[i] == -1) {
            printf("load4EKATEParams: Invalid offset for %s\n", chekateFilesPaths[i]);
            return;
        }
    }
    FsFile file;
    Result rc = fsFsOpenFile(&sdFs, chekateFilesPaths[0], FsOpenMode_Read, &file);
    if (R_FAILED(rc)) {
        printf("load4EKATEParams: Failed to open %s: 0x%x\n", chekateFilesPaths[0], rc);
        return;
    }
    u64 bytesRead;
    rc = fsFileRead(&file, paramsOffsets[0], params, sizeof(CHEKATEParams), 0, &bytesRead);
    fsFileClose(&file);
    if (R_FAILED(rc)) {
        printf("load4EKATEParams: Read failed: 0x%x\n", rc);
    }
}

bool chekateFilesExists() {
    for (int i = 0; i < CHEKATE_FILES_COUNT; ++i) {
        FsDirEntryType type;
        Result rc = fsFsGetEntryType(&sdFs, chekateFilesPaths[i], &type);
        if (R_FAILED(rc) || type != FsDirEntryType_File) {
            printf("chekateFilesExists: File missing: %s (rc: 0x%x)\n", chekateFilesPaths[i], rc);
            return false;
        }
    }
    return true;
}

bool is4IFIXPresent() {
    FsFile file;
    Result rc = fsFsOpenFile(&sdFs, chekateFilesPaths[0], FsOpenMode_Read, &file);
    if (R_FAILED(rc)) {
        printf("is4IFIXPresent: Failed to open %s: 0x%x\n", chekateFilesPaths[0], rc);
        return false;
    }

    s64 offset = searchBytesArray(&file, chifixPattern, sizeof(chifixPattern), 0);
    fsFileClose(&file);
    if (offset != -1) {
        printf("is4IFIXPresent: 4IFIX detected at offset 0x%lx\n", offset);
        return true;
    }
    printf("is4IFIXPresent: 4IFIX not detected\n");
    return false;
}

bool patch4ekateStage(int stageId) {
    if (stageId < 0 || stageId >= CHEKATE_STAGES_COUNT) {
        printf("patch4ekateStage: Invalid stage ID: %d\n", stageId);
        return false;
    }

    if (is4IFIXPresent()) {
        printf("patch4ekateStage: 4IFIX is active, aborting\n");
        return false;
    }

    if (!chekateFilesExists()) {
        printf("patch4ekateStage: Some files are missing\n");
        return false;
    }

    for (int i = 0; i < CHEKATE_FILES_COUNT; ++i) {
        if (paramsOffsets[i] == -1) {
            printf("patch4ekateStage: Invalid offset for %s\n", chekateFilesPaths[i]);
            return false;
        }
    }

    CHEKATEParams params = stages[stageId];
    for (int i = 0; i < CHEKATE_FILES_COUNT; ++i) {
        FsFile file;
        Result rc = fsFsOpenFile(&sdFs, chekateFilesPaths[i], FsOpenMode_Write, &file);
        if (R_FAILED(rc)) {
            printf("patch4ekateStage: Failed to open %s for writing: 0x%x\n", chekateFilesPaths[i], rc);
            return false;
        }
        rc = fsFileWrite(&file, paramsOffsets[i], &params, sizeof(CHEKATEParams), 0);
        fsFileClose(&file);
        if (R_FAILED(rc)) {
            printf("patch4ekateStage: Failed to write to %s: 0x%x\n", chekateFilesPaths[i], rc);
            return false;
        }
        printf("patch4ekateStage: Patched %s at offset 0x%lx\n", chekateFilesPaths[i], paramsOffsets[i]);
    }

    if (!chekateStageWasChanged) chekateStageWasChanged = true;
    return true;
}

int getCurrentStageId() {
    if (is4IFIXPresent()) {
        printf("getCurrentStageId: 4IFIX detected\n");
        return -2;
    }

    for (int i = 0; i < CHEKATE_FILES_COUNT; ++i) {
        if (paramsOffsets[i] == -1) {
            printf("getCurrentStageId: Invalid offset for %s\n", chekateFilesPaths[i]);
            return -1;
        }
    }

    CHEKATEParams* params = (CHEKATEParams*)malloc(sizeof(CHEKATEParams));
    if (!params) {
        printf("getCurrentStageId: Memory allocation failed\n");
        return -1;
    }

    load4EKATEParams(params);
    for (int i = 0; i < CHEKATE_STAGES_COUNT; ++i) {
        if (compareU8Arrays((u8*)params, (u8*)&stages[i], sizeof(CHEKATEParams))) {
            free(params);
            printf("getCurrentStageId: Stage %d detected\n", i);
            return i;
        }
    }
    free(params);
    printf("getCurrentStageId: No matching stage found\n");
    return -1;
}

const char* getCurrentStageTitle() {
    const int stageId = getCurrentStageId();
    if (stageId == -1) {
        printf("getCurrentStageTitle: Returning UNKNOWN\n");
        return CHEKATE_UNKNOWN_STAGE;
    } else if (stageId == -2) {
        printf("getCurrentStageTitle: Returning 4IFIX\n");
        return chifixTitle;
    }
    printf("getCurrentStageTitle: Returning stage %d\n", stageId);
    return stagesTitles[stageId];
}