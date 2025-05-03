#pragma once

#include <tesla.hpp>
#include <utils.hpp>

class KipInfoOverlay : public tsl::Gui {
private:
    std::vector<std::string> kipInfoCommand;
    bool showBackup;
    int pageIndex = 0;  // 0 = первая страница, 1 = вторая
    bool hasPages;      // Определяет, доступны ли дополнительные страницы
    bool isJsonKipSettings = false;
    tsl::elm::List* p_list;

public:
    KipInfoOverlay(const std::vector<std::string>& kipInfoCommand)
        : kipInfoCommand(kipInfoCommand), showBackup(true), pageIndex(0), hasPages(false), isJsonKipSettings(false), p_list(nullptr) {
        initializePages();
    }
    
    KipInfoOverlay(const std::vector<std::string>& kipInfoCommand, bool showBackup, int pageIdx = 0)
        : kipInfoCommand(kipInfoCommand), showBackup(showBackup), pageIndex(pageIdx), hasPages(false), isJsonKipSettings(false), p_list(nullptr) {
        initializePages();
    }
    
    ~KipInfoOverlay() {}

private:
    // Инициализация доступности страниц
    void initializePages() {
        if (showBackup) {
            hasPages = (kipInfoCommand.size() >= 3); // Только две страницы при 3+ элементах
        } else {
            hasPages = (kipInfoCommand.size() >= 3); // Две страницы при 3 элементах, три страницы при 4+
        }
    }

public:
    virtual tsl::elm::Element* createUI() override {
        std::pair<std::string, int> textDataPair;
        constexpr int lineHeight = 20;
        constexpr int fontSize = 19;
        std::string footer;

        // Определение футера
        if (showBackup) {
            footer = (kipInfoCommand.size() >= 3 && pageIndex == 0) ? "\uE0E0  Apply     \uE0E2  Delete     \uE0EE  Page 2" :
                     (kipInfoCommand.size() >= 3 && pageIndex == 1) ? "\uE0E0  Apply     \uE0E2  Delete     \uE0ED  Page 1" :
                     "\uE0E0  Apply     \uE0E2  Delete";
        } else {
            footer = (kipInfoCommand.size() >= 4 && pageIndex == 0) ? "\uE0E1  Back     \uE0EE  Page 2" :
                     (kipInfoCommand.size() >= 4 && pageIndex == 1) ? "\uE0E1  Back     \uE0ED  Page 1     \uE0EE  Page 3" :
                     (kipInfoCommand.size() >= 4 && pageIndex == 2) ? "\uE0E1  Back     \uE0ED  Page 2" :
                     (kipInfoCommand.size() == 3 && pageIndex == 0) ? "\uE0E1  Back     \uE0EE  Page 2" :
                     (kipInfoCommand.size() == 3 && pageIndex == 1) ? "\uE0E1  Back     \uE0ED  Page 1" :
                     "\uE0E1  Back";
        }

        auto rootFrame = new tsl::elm::OverlayFrame("Kip Management", "Uberhand Package", "", false, footer);
        auto list = new tsl::elm::List();
        p_list = list;

        // Получение данных
        if (!showBackup) {
            if (pageIndex == 2) {
                textDataPair = (kipInfoCommand.size() <= 3) ? std::make_pair("Error: Invalid configuration for page 3", 1) :
                               dispKipIniData(kipInfoCommand[3]);
            } else {
                textDataPair = (kipInfoCommand.size() <= static_cast<size_t>(pageIndex + 1)) ?
                               std::make_pair("Error: Invalid configuration for page " + std::to_string(pageIndex + 1), 1) :
                               dispCustData(kipInfoCommand[pageIndex + 1]);
            }
        } else {
            if (kipInfoCommand.size() <= 1) {
                textDataPair = {"Error: Invalid configuration for backup mode", 1};
            } else if (kipInfoCommand[1].find(".json") != std::string::npos) {
                isJsonKipSettings = true;
                textDataPair = (pageIndex == 2) ? std::make_pair("Error: Page 3 not available in backup mode", 1) :
                               dispKipCustomDataFromJson(kipInfoCommand[1], pageIndex + 1);
            } else {
                textDataPair = (kipInfoCommand.size() <= static_cast<size_t>(pageIndex + 2)) ?
                               std::make_pair("Error: Invalid configuration for page " + std::to_string(pageIndex + 1) + " (backup with kip)", 1) :
                               dispCustData(kipInfoCommand[pageIndex + 2], kipInfoCommand[1]);
            }
        }

        std::string textdata = textDataPair.first;
        int textsize = textDataPair.second;

        if (!textdata.empty()) {
            list->addItem(new tsl::elm::CustomDrawer([lineHeight, fontSize, textdata](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
                renderer->drawString(textdata.c_str(), false, x, y + lineHeight, fontSize, a(tsl::style::color::ColorText));
            }), fontSize * textsize + lineHeight);
            rootFrame->setContent(list);
        } else {
            list->addItem(new tsl::elm::ListItem("Error: No data to display"));
            rootFrame->setContent(list);
        }

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

        // Обработка кнопок
        if (keysDown & KEY_B) {
            // Выполняем goback нужное количество раз в зависимости от страницы
            for (int i = 0; i <= pageIndex; i++) {
                tsl::goBack();
            }
            return true;
        }
        if (showBackup && (keysDown & KEY_A)) {
            if (isJsonKipSettings && pageIndex != 2 && kipInfoCommand.size() > 3) {
                std::vector<std::string> commands = {kipInfoCommand[2], kipInfoCommand[3]};
                setCurrentKipCustomDataFromJson(kipInfoCommand[1], commands);
            } else if (pageIndex != 2) {
                copyFileOrDirectory(kipInfoCommand[1], "/atmosphere/kips/loader.kip");
            }
            for (int i = 0; i <= pageIndex; i++) {
                tsl::goBack();
            }
            return true;
        }
        if (showBackup && (keysDown & KEY_X)) {
            deleteFileOrDirectory(kipInfoCommand[1]);
            for (int i = 0; i <= pageIndex; i++) {
                tsl::goBack();
            }
            return true;
        }
        if (hasPages && (keysDown & KEY_DRIGHT)) {
            // Ограничиваем переход на третью страницу при showBackup
            if (pageIndex < 1 || (!showBackup && pageIndex == 1 && kipInfoCommand.size() >= 4)) {
                tsl::changeTo<KipInfoOverlay>(kipInfoCommand, showBackup, pageIndex + 1);
                return true;
            }
        }
        if (hasPages && (keysDown & KEY_DLEFT)) {
            if (pageIndex > 0) {
                tsl::goBack();
                return true;
            }
        }
        return false;
    }
};