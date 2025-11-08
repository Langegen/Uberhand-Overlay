#pragma once

#include <tesla.hpp>
#include "4ekate.hpp"
#include <string.h>

extern FsFileSystem sdFs;
extern bool fsInitialized;

class FourEkateOverlay : public tsl::Gui {
private:
    tsl::elm::List* p_list;

public:
    FourEkateOverlay() : p_list(nullptr) {}

    virtual tsl::elm::Element* createUI() override {
        // Инициализация файловой системы
        if (!fsInitialized) {
            Result rc = fsInitialize();
            if (R_SUCCEEDED(rc)) {
                rc = fsOpenSdCardFileSystem(&sdFs);
                if (R_SUCCEEDED(rc)) {
                    fsInitialized = true;
                }
            }
        }
        // Инициализация стейджей
        set4ekateStagesOffsets();

        // Создание основного фрейма с заголовком и футером
        auto rootFrame = new tsl::elm::OverlayFrame("4ekate Management", "Uberhand Package", "", false, "\uE0E1 Back");
        p_list = new tsl::elm::List();

        // Разделитель: Current Stage
        p_list->addItem(new tsl::elm::CategoryHeader("Current Stage"));

        // Получение текущего стейджа
        const char* currentStage = getCurrentStageTitle();
        auto currentStageItem = new tsl::elm::ListItem(currentStage);
        currentStageItem->setValue("\u25CF", tsl::PredefinedColors::Gray); // Серый цвет для неактивного элемента
        p_list->addItem(currentStageItem);

        // Разделитель: Select Stage
        p_list->addItem(new tsl::elm::CategoryHeader("Select Stage"));

        // Проверка, является ли текущий стейдж UNKNOWN или 4IFIX
        bool isUnknownOr4IFIX = (strcmp(currentStage, CHEKATE_UNKNOWN_STAGE) == 0 || is4IFIXPresent());

        // Проверка наличия файлов 4ekate
        // bool filesExist = chekateFilesExists();
        // Временно отключить для тестирования
        // if (!filesExist) {
        //     auto errorItem = new tsl::elm::ListItem("Error: 4ekate files missing");
        //     errorItem->setValue("\u25CF", tsl::PredefinedColors::Gray);
        //     p_list->addItem(errorItem);
        // } else
        if (isUnknownOr4IFIX) {
            auto errorItem = new tsl::elm::ListItem("4IFIX active");
            errorItem->setValue("\u25CF", tsl::PredefinedColors::Gray);
            p_list->addItem(errorItem);
        } else {
            // Добавление элементов для выбора стейджей
            for (int i = 0; i < CHEKATE_STAGES_COUNT; ++i) {
                auto stageItem = new tsl::elm::ListItem(stagesTitles[i]);
                stageItem->setClickListener([i, this](u64 keys) -> bool {
                    if (keys & KEY_A) {
                        if (patch4ekateStage(i)) {
                            tsl::changeTo<FourEkateOverlay>();
                            return true;
                        }
                        return false;
                    }
                    return false;
                });
                p_list->addItem(stageItem);
            }
        }

        // Разделитель: Reboot
        p_list->addItem(new tsl::elm::CategoryHeader("Reboot"));

        // Кнопка перезагрузки
        auto rebootItem = new tsl::elm::ListItem("Reboot");
        rebootItem->setClickListener([](u64 keys) -> bool {
            if (keys & KEY_A) {
                splExit();
                fsdevUnmountAll();
                spsmShutdown(SpsmShutdownMode_Reboot);
                return true;
            }
            return false;
        });
        p_list->addItem(rebootItem);

        rootFrame->setContent(p_list);
        return rootFrame;
    }

    virtual bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        // Прокрутка правым стиком
        if (keysHeld & HidNpadButton_StickRDown) {
            if (p_list) p_list->handleInput(HidNpadButton_StickRDown, 0, {}, {}, {});
            return true;
        }
        if (keysHeld & HidNpadButton_StickRUp) {
            if (p_list) p_list->handleInput(HidNpadButton_StickRUp, 0, {}, {}, {});
            return true;
        }

        // Обработка кнопки "Назад"
        if (keysDown & KEY_B) {
            tsl::goBack();
            return true;
        }
        return false;
    }
};