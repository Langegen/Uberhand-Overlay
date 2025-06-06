#define NDEBUG
#define STBTT_STATIC
#define TESLA_INIT_IMPL

#include <FanSliderOverlay.hpp>
#include <KipInfoOverlay.hpp>
#include <switch/kernel/thread.h>
#include <tesla.hpp>
#include <utils.hpp>

// Overlay booleans
static bool defaultMenuLoaded = true;
static std::string package = "";
bool enableConfigNav = false;
bool showCurInMenu = false;
std::string kipVersion = "";
bool DownloadProcessing = false;
bool sameKeyCombo = false;
bool exitMT = false;
bool Mtrun = false;
bool isAltMenu = false;
int errCode = -2;
std::string progress;
Thread threadMT;

enum Screen {
    Default,
    Overlays,
    Packages,
    Soverlays,
    Spackages
};

class ConfigOverlay : public tsl::Gui {
private:
    std::string filePath, specificKey;
    bool isInSection, inQuotes;
    bool useAlt;

public:
    ConfigOverlay(const std::string& file, const std::string& key = "", bool alt = false)
        : filePath(file), specificKey(key), useAlt(alt)
    {
    }
    ~ConfigOverlay() { }

    virtual tsl::elm::Element* createUI() override
    {
        // log ("ConfigOverlay");
        auto rootFrame = new tsl::elm::OverlayFrame(getNameFromPath(filePath), "Uberhand Config");
        auto list = new tsl::elm::List();

        std::string configFile = filePath + "/" + (useAlt ? "alt_config.ini" : configFileName);

        std::string fileContent = getFileContents(configFile);
        if (!fileContent.empty()) {
            std::string line;
            std::istringstream iss(fileContent);
            std::string currentCategory;
            isInSection = false;
            while (std::getline(iss, line)) {
                if (line.empty() || line.find_first_not_of('\n') == std::string::npos) {
                    continue;
                }

                if (line.front() == '[' && line.back() == ']') {
                    if (!specificKey.empty()) {
                        if (line.substr(1, line.size() - 2) == specificKey) {
                            currentCategory = line.substr(1, line.size() - 2);
                            isInSection = true;
                            list->addItem(new tsl::elm::CategoryHeader(currentCategory));
                        } else {
                            currentCategory.clear();
                            isInSection = false;
                        }
                    } else {
                        currentCategory = line.substr(1, line.size() - 2);
                        isInSection = true;
                        list->addItem(new tsl::elm::CategoryHeader(currentCategory));
                    }
                } else if (isInSection) {
                    auto listItem = new tsl::elm::ListItem(line);
                    listItem->setClickListener([line, this, listItem](uint64_t keys) {
                        if (keys & KEY_A) {
                            std::istringstream iss(line);
                            std::string part;
                            std::vector<std::vector<std::string>> commandVec;
                            std::vector<std::string> commandParts;
                            inQuotes = false;

                            while (std::getline(iss, part, '\'')) {
                                if (!part.empty()) {
                                    if (!inQuotes) {
                                        std::istringstream argIss(part);
                                        std::string arg;
                                        while (argIss >> arg) {
                                            commandParts.emplace_back(arg);
                                        }
                                    } else {
                                        commandParts.emplace_back(part);
                                    }
                                }
                                inQuotes = !inQuotes;
                            }

                            commandVec.emplace_back(std::move(commandParts));
                            int result = interpretAndExecuteCommand(commandVec);
                            if (result == 0) {
                                listItem->setValue("DONE", tsl::PredefinedColors::Green);
                            } else if (result == 1) {
                                tsl::goBack();
                            } else {
                                listItem->setValue("FAIL", tsl::PredefinedColors::Red);
                            }
                            return true;
                        } else if (keys && (listItem->getValue() == "DONE" || listItem->getValue() == "FAIL")) {
                            listItem->setValue("");
                        }
                        return false;
                    });
                    list->addItem(listItem);
                }
            }
        } else {
            list->addItem(new tsl::elm::ListItem("Failed to open file: " + configFile));
        }

        rootFrame->setContent(list);
        return rootFrame;
    }

    bool adjuct_top, adjuct_bot = false;

    virtual bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override
    {
        if (this->adjuct_top) { // Adjust cursor to the top item after jump bot-top
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            this->adjuct_top = false;
            return true;
        }
        if (this->adjuct_bot) { // Adjust cursor to the top item after jump top-bot
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            this->adjuct_bot = false;
            return true;
        }
        // Jump bot-top: scroll to the top after hitting the last list item
        if ((keysDown & KEY_DDOWN) || (keysDown & KEY_DOWN)) {
            auto prevItem = this->getFocusedElement();
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            if (prevItem == this->getFocusedElement()) {
                scrollListItems(this, ShiftFocusMode::UpMax);
                this->adjuct_top = true; // Go one item
            } else { // Adjust to account for tesla key processing
                this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            }
            return false;
        }
        // Jump bot-top: scroll to the top after hitting the last list item
        if ((keysDown & KEY_DUP) || (keysDown & KEY_UP)) {
            auto prevItem = this->getFocusedElement();
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            if (prevItem == this->getFocusedElement()) {
                scrollListItems(this, ShiftFocusMode::DownMax);
                this->adjuct_bot = true;
            } else { // Adjust to account for tesla key processing
                this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            }
            return false;
        }
        if (keysDown & KEY_L) { // Scroll to up for 5 items
            scrollListItems(this, ShiftFocusMode::UpNum);
            return true;
        }
        if (keysDown & KEY_R) { // Scroll to down for 5 items
            scrollListItems(this, ShiftFocusMode::DownNum);
            return true;
        }
        if (keysDown & KEY_ZL) { // Scroll to up for 5 items
            scrollListItems(this, ShiftFocusMode::UpMax);
            return true;
        }
        if (keysDown & KEY_ZR) { // Scroll to down for 5 items
            scrollListItems(this, ShiftFocusMode::DownMax);
            return true;
        }
        if (keysDown & KEY_B) {
            if (!Mtrun) {
                tsl::goBack();
            }
            return true;
        }
        return false;
    }
};

class CustomCategoryHeader : public tsl::elm::CategoryHeader {
private:
    tsl::Color m_textColor;
    bool m_hasSeparator;
    std::vector<std::string> m_lines;
    static constexpr size_t maxCharsPerLine = 44;
    static constexpr s32 textSize = 15;
    static constexpr s32 lineHeight = textSize + 3;
    static constexpr s32 paddingLeft = 13;
    static constexpr s32 spacing = 5;
    static constexpr s32 paddingTop = 5;
    bool m_showSeparator = true;

    void splitText(const std::string& title) {
        m_lines.clear();
        std::string remaining = title;
        while (!remaining.empty()) {
            size_t charsToTake = std::min(maxCharsPerLine, remaining.length());
            size_t splitPos = charsToTake;
            if (charsToTake < remaining.length()) {
                size_t lastSpace = remaining.find_last_of(' ', charsToTake);
                if (lastSpace != std::string::npos && lastSpace > 0) {
                    splitPos = lastSpace;
                }
            }
            m_lines.push_back(remaining.substr(0, splitPos));
            remaining = (splitPos < remaining.length()) ? remaining.substr(splitPos + 1) : "";
        }
    }

public:
    CustomCategoryHeader(const std::string& title, bool hasSeparator = false, tsl::Color textColor = tsl::style::color::ColorText)
        : tsl::elm::CategoryHeader(title, hasSeparator), m_textColor(textColor), m_hasSeparator(hasSeparator) {
        splitText(title);
        m_showSeparator = readIniValue("/config/uberhand/config.ini", "uberhand", "show_separator") != "false";
    }

    void setText(const std::string& title) {
        tsl::elm::CategoryHeader::setText(title);  // Исправленный вызов родительского метода
        splitText(title);
    }

    void draw(tsl::gfx::Renderer* renderer) override {
        const s32 rectX = this->getX();
        const s32 rectY = this->getY();
        const s32 totalTextHeight = m_lines.size() * lineHeight;
        const s32 elementHeight = totalTextHeight + spacing + paddingTop;

        // Голубая вертикальная полоса с возвращенной шириной и уменьшенной высотой
        tsl::Color lightBlue(0x6, 0xB, 0xF, 0xF); // #66B2FF
        renderer->drawRect(rectX - 2, rectY, 5, elementHeight - 2, renderer->a(lightBlue));

        // Вычисляем yOffset для строгого центрирования текста
        s32 yOffset = rectY + (elementHeight - totalTextHeight) / 2 + textSize;

        for (const auto& line : m_lines) {
            renderer->drawString(line.c_str(), false, rectX + paddingLeft, yOffset, textSize, renderer->a(m_textColor));
            yOffset += lineHeight;
        }

        // Горизонтальная линия-разделитель внутри границ элемента
        if (m_hasSeparator && m_showSeparator) {
            renderer->drawRect(rectX, rectY + elementHeight - 1, this->getWidth(), 1, renderer->a(tsl::style::color::ColorFrame));
        }
    }

    void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
        this->setBoundaries(this->getX(), this->getY(), this->getWidth(), getHeight(parentWidth));
    }

    bool onClick(u64 keys) override {
        return false;
    }

    tsl::elm::Element* requestFocus(tsl::elm::Element* oldFocus, tsl::FocusDirection direction) override {
        return nullptr;
    }

    u16 getHeight(u16 parentWidth) const {
        size_t lines = m_lines.empty() ? 1 : m_lines.size();
        u16 height = static_cast<u16>(lines * lineHeight + spacing + paddingTop);
        return height;
    }
};

class SelectionOverlay : public tsl::Gui {
private:
    std::string filePath, specificKey, pathPattern, pathPatternOn, pathPatternOff, jsonPath, jsonKey, itemName, parentDirName, lastParentDirName, textPath, footer;
    std::vector<std::string> filesList, filesListOn, filesListOff, filterList, filterOnList, filterOffList;
    std::vector<std::vector<std::string>> commands;
    bool toggleState = false;
    bool searchCurrent;
    tsl::elm::ListItem* savedItem = nullptr;
    tsl::elm::List* p_list;
    bool useText = false;

public:
    SelectionOverlay(const std::string& file, const std::string& key = "", const std::vector<std::vector<std::string>>& cmds = {}, std::string footer = "")
        : filePath(file)
        , specificKey(key)
        , footer(footer)
        , commands(cmds)
    {
    }
    ~SelectionOverlay() { }

    virtual tsl::elm::Element* createUI() override
    {
        // log ("SelectionOverlay");

        bool hasHelp = false;
        std::string helpPath;
        std::string menuName;
        size_t fifthSlashPos = filePath.find('/', filePath.find('/', filePath.find('/', filePath.find('/') + 1) + 1) + 1);
        if (fifthSlashPos != std::string::npos) {
            // Extract the substring up to the fourth slash
            helpPath = filePath.substr(0, fifthSlashPos);
            if (!specificKey.empty()) {
                helpPath += "/Help/" + getNameFromPath(filePath) + "/" + specificKey + ".txt";
            } else {
                helpPath += "/Help/" + getNameFromPath(filePath) + ".txt";
            }
            if (isFileOrDirectory(helpPath)) {
                hasHelp = true;
            } else {
                helpPath = "";
            }
        }
        auto rootFrame = new tsl::elm::OverlayFrame(specificKey.empty() ? getNameFromPath(filePath) : specificKey,
                                                    "Uberhand Package", "", hasHelp, footer);
        auto list = new tsl::elm::List();

        bool useFilter = false;
        bool useSource = false;
        bool useJson = false;
        bool useToggle = false;
        bool useSplitHeader = false;
        bool markCurKip = false;
        bool markCurIni = false;
        std::string ramPath = "";
        std::string sourceIni = "";
        std::string sectionIni = "";
        std::string keyIni = "";
        std::string offset = "";
        std::pair<std::string, int> textDataPair;

        constexpr int lineHeight = 20; // Adjust the line height as needed
        constexpr int fontSize = 19; // Adjust the font size as needed

        for (const auto& cmd : commands) {
            if (cmd.size() > 1) {
                if (cmd[0] == "split") {
                    useSplitHeader = true;
                } else if (cmd[0] == "filter") {
                    filterList.push_back(cmd[1]);
                    useFilter = true;
                } else if (cmd[0] == "filter_on") {
                    filterOnList.push_back(cmd[1]);
                    useToggle = true;
                } else if (cmd[0] == "filter_off") {
                    filterOffList.push_back(cmd[1]);
                    useToggle = true;
                } else if (cmd[0] == "source") {
                    pathPattern = cmd[1];
                    useSource = true;
                } else if (cmd[0] == "source_on") {
                    pathPatternOn = cmd[1];
                    useToggle = true;
                } else if (cmd[0] == "source_off") {
                    pathPatternOff = cmd[1];
                    useToggle = true;
                } else if (cmd[0] == "json_source") {
                    jsonPath = preprocessPath(cmd[1]);
                    if (cmd.size() > 2) {
                        jsonKey = cmd[2]; //json display key
                    }
                    useJson = true;
                } else if (cmd[0] == "kip_info") {
                    jsonPath = preprocessPath(cmd[1]);
                } else if (cmd[0] == "json_mark_cur_kip") {
                    jsonPath = preprocessPath(cmd[1]);
                    if (cmd.size() > 2) {
                        jsonKey = cmd[2]; //json display key
                    }
                    useJson = true;
                    if (cmd.size() > 3) {
                        offset = cmd[3];
                        markCurKip = true;
                    }
                } else if (cmd[0] == "json_mark_cur_ini") {
                    jsonPath = preprocessPath(cmd[1]);
                    if (cmd.size() > 2) {
                        jsonKey = cmd[2]; //json display key
                    }
                    useJson = true;
                    if (cmd.size() > 5) { // Enough parts are provided to mark the current ini value
                        sourceIni = preprocessPath(cmd[3]);
                        sectionIni = cmd[4];
                        keyIni = cmd[5];
                        markCurIni = true;
                    }
                } else if (cmd[0] == "text_source") {
                    textPath = preprocessPath(cmd[1]);
                    this->useText = true;
                }
            }
        }

        // Get the list of files matching the pattern

        if (!useToggle) {
            if (this->useText) {
                p_list = list;
                if (!isFileOrDirectory(textPath)) {
                    list->addItem(new tsl::elm::CustomDrawer([lineHeight, fontSize](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
                                      renderer->drawString("Text file not found.\nContact the package dev.", false, x, y + lineHeight, fontSize, a(tsl::style::color::ColorText));
                                  }),
                                  fontSize + lineHeight);
                    rootFrame->setContent(list);
                    return rootFrame;
                } else {
                    textDataPair = readTextFromFile(textPath);
                    std::string textdata = textDataPair.first;
                    int textsize = textDataPair.second;
                    if (!textdata.empty()) {
                        list->addItem(new tsl::elm::CustomDrawer([lineHeight, fontSize, textdata](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
                                          renderer->drawString(textdata.c_str(), false, x, y + lineHeight, fontSize, a(tsl::style::color::ColorText));
                                      }),
                                      fontSize * textsize + lineHeight);
                        rootFrame->setContent(list);
                        return rootFrame;
                    }
                }
            } else if (useJson) {
                if (!isFileOrDirectory(jsonPath)) {
                    list->addItem(new tsl::elm::CustomDrawer([lineHeight, fontSize](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
                                      renderer->drawString("JSON file not found.\nContact the package dev.", false, x, y + lineHeight, fontSize, a(tsl::style::color::ColorText));
                                  }),
                                  fontSize + lineHeight);
                    rootFrame->setContent(list);
                    return rootFrame;
                } else {
                    std::string currentHex = ""; // Is used to mark current value from the kip
                    bool detectSize = true;
                    searchCurrent = markCurKip || markCurIni ? true : false;
                    // create list of data in the json
                    auto jsonData = readJsonFromFile(jsonPath);
                    if (jsonData && json_is_array(jsonData)) {
                        size_t arraySize = json_array_size(jsonData);
                        for (size_t i = 0; i < arraySize; ++i) {
                            json_t* item = json_array_get(jsonData, i);
                            if (item && json_is_object(item)) {
                                json_t* keyValue = json_object_get(item, jsonKey.c_str());
                                if (keyValue && json_is_string(keyValue)) {
                                    std::string name;
                                    json_t* hexValue = json_object_get(item, "hex");
                                    json_t* decValue = json_object_get(item, "dec");
                                    json_t* otherValue = json_object_get(item, "value");
                                    bool hexOrDec = (hexValue || decValue);
                                    bool hexOrDecOrVal = (hexOrDec || otherValue);
                                    json_t* colorValue = json_object_get(item, "color");
                                    if (markCurKip && hexOrDec && searchCurrent) {
                                        const char* valueStr;
                                        int hexLength;
                                        if (hexValue) {
                                            valueStr = json_string_value(hexValue);
                                            hexLength = strlen(valueStr) / 2;
                                            hexLength = std::max(hexLength, 1);
                                        } else if (decValue) {
                                            valueStr = json_string_value(decValue);
                                            hexLength = 4;
                                        }
                                        if (detectSize) {
                                            try {
                                                detectSize = false;
                                                const std::string CUST = "43555354";
                                                currentHex = readHexDataAtOffset("/atmosphere/kips/loader.kip", CUST, std::stoul(offset), hexLength); // Read the data from kip with offset starting from 'C' in 'CUST'
                                                if (decValue) {
                                                    currentHex = std::to_string(reversedHexToInt(currentHex));
                                                }
                                            } catch (const std::invalid_argument& ex) {
                                                log("ERROR - %s:%d  - invalid offset value: \"%s\" in \"%s\"", __func__, __LINE__, offset.c_str(), jsonPath.c_str());
                                            }
                                        }
                                        if (valueStr == currentHex) {
                                            name = json_string_value(keyValue);
                                            if (name.find(" - ") != std::string::npos) {
                                                name = name + " | " + checkmarkChar;
                                            } else {
                                                name = name + " - " + checkmarkChar;
                                            }
                                            searchCurrent = false;
                                        } else {
                                            name = json_string_value(keyValue);
                                        }
                                    } else if (markCurIni && hexOrDecOrVal && searchCurrent) {
                                        const char* iniValueStr;
                                        if (hexValue) {
                                            iniValueStr = json_string_value(hexValue);
                                        } else if (decValue) {
                                            iniValueStr = json_string_value(decValue);
                                        } else {
                                            iniValueStr = json_string_value(otherValue);
                                        }
                                        std::string iniValue = readIniValue(sourceIni, sectionIni, keyIni);
                                        if (iniValueStr == iniValue) {
                                            name = json_string_value(keyValue);
                                            if (name.find(" - ") != std::string::npos) {
                                                name = name + " | " + checkmarkChar;
                                            } else {
                                                name = name + " - " + checkmarkChar;
                                            }
                                            searchCurrent = false;
                                        } else {
                                            name = json_string_value(keyValue);
                                        }
                                    } else {
                                        name = json_string_value(keyValue);
                                    }
                                    if (colorValue) {
                                        name = name + " ::" + json_string_value(colorValue);
                                    }
                                    filesList.push_back(name);
                                }
                            }
                        }
                    }
                }
            } else if (useFilter || useSource) {
                filesList = getFilesListByWildcards(pathPattern);
                std::sort(filesList.begin(), filesList.end(), [](const std::string& a, const std::string& b) {
                    return getNameFromPath(a) < getNameFromPath(b);
                });
            }
        } else {
            filesListOn = getFilesListByWildcards(pathPatternOn);
            filesListOff = getFilesListByWildcards(pathPatternOff);

            // Apply On Filter
            for (const auto& filterOnPath : filterOnList) {
                removeEntryFromList(filterOnPath, filesListOn);
            }
            // Apply Off Filter
            for (const auto& filterOnPath : filterOffList) {
                removeEntryFromList(filterOnPath, filesListOff);
            }

            // remove filterOnPath from filesListOn
            // remove filterOffPath from filesListOff

            filesList.reserve(filesListOn.size() + filesListOff.size());
            filesList.insert(filesList.end(), filesListOn.begin(), filesListOn.end());
            filesList.insert(filesList.end(), filesListOff.begin(), filesListOff.end());
            if (useSplitHeader) {
                std::sort(filesList.begin(), filesList.end(), [](const std::string& a, const std::string& b) {
                    std::string parentDirA = getParentDirNameFromPath(a);
                    std::string parentDirB = getParentDirNameFromPath(b);

                    // Compare parent directory names
                    if (parentDirA != parentDirB) {
                        return parentDirA < parentDirB;
                    } else {
                        // Parent directory names are the same, compare filenames
                        std::string filenameA = getNameFromPath(a);
                        std::string filenameB = getNameFromPath(b);

                        // Compare filenames
                        return filenameA < filenameB;
                    }
                });
            } else {
                std::sort(filesList.begin(), filesList.end(), [](const std::string& a, const std::string& b) {
                    return getNameFromPath(a) < getNameFromPath(b);
                });
            }
        }

        // Apply filter
        for (const auto& filterPath : filterList) {
            removeEntryFromList(filterPath, filesList);
        }

        // Add each file as a menu item
        int count = 0;
        std::string jsonSep = "";
        bool isFirst = true;
        bool hasSep = false;
        for (const std::string& file : filesList) {
            //if (file.compare(0, filterPath.length(), filterPath) != 0){
            itemName = getNameFromPath(file);
            if (!isDirectory(preprocessPath(file))) {
                itemName = dropExtension(itemName);
            }
            parentDirName = getParentDirNameFromPath(file);
            if (useSplitHeader && (lastParentDirName.empty() || (lastParentDirName != parentDirName))) {
                list->addItem(new CustomCategoryHeader(removeQuotes(parentDirName)));
                lastParentDirName = parentDirName;
            }

            if (!useToggle) {
                std::string color = "Default";
                if (useJson) { // For JSON wildcards
                    size_t pos = file.find(" - ");
                    size_t pos2 = file.find("`");
                    size_t colorPos = file.find(" ::");
                    std::string footer = "";
                    std::string optionName = file;
                    if (colorPos != std::string::npos) {
                        color = file.substr(colorPos + 3);
                        optionName = file.substr(0, colorPos);
                    }
                    if (pos != std::string::npos) {
                        footer = optionName.substr(pos + 2); // Assign the part after "&&" as the footer
                        optionName.resize(pos); // Strip the "&&" and everything after it
                    }
                    if (pos2 == 0) { // separator
                        if (isFirst) {
                            jsonSep = optionName.substr(1);
                            hasSep = true;
                        } else {
                            auto item = new CustomCategoryHeader(optionName.substr(1), true);
                            list->addItem(item);
                        }
                    } else {
                        isFirst = false;
                        auto listItem = new tsl::elm::ListItem(optionName);
                        listItem->setValue(footer);
                        listItem->setClickListener([count, this, listItem, helpPath, footer](uint64_t keys) { // Add 'command' to the capture list
                            if (keys & KEY_A) {
                                if (listItem->getValue() == "APPLIED" && !prevValue.empty()) {
                                    listItem->setValue(prevValue);
                                    prevValue = "";
                                    resetValue = false;
                                }
                                if (listItem->getValue() != "DELETED") {
                                    // Replace "{json_source}" with file in commands, then execute
                                    std::string countString = std::to_string(count);
                                    std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(commands, countString, false, true, true);
                                    int result = interpretAndExecuteCommand(modifiedCommands);
                                    if (result == 0) {
                                        listItem->setValue("DONE", tsl::PredefinedColors::Green);
                                    } else if (result == 1) {
                                        applied = true;
                                        prevValue = listItem->getText();
                                        tsl::goBack();
                                    } else {
                                        listItem->setValue("FAIL", tsl::PredefinedColors::Red);
                                    }
                                }
                                return true;
                            } else if (keys & KEY_Y && !helpPath.empty()) {
                                tsl::changeTo<HelpOverlay>(helpPath);
                            } else if (keys && (listItem->getValue() == "DONE" || listItem->getValue() == "FAIL")) {
                                listItem->setValue(footer);
                            }
                            return false;
                        });
                        if (color.compare(0, 1, "#") == 0) {
                            listItem->setColor(tsl::PredefinedColors::Custom, color);
                        } else {
                            listItem->setColor(defineColor(color));
                        }
                        // Find the curent list item to jump to
                        size_t checkmarkPos = listItem->getValue().find(checkmarkChar);
                        if (checkmarkPos != std::string::npos) {
                            savedItem = listItem;
                        }
                        list->addItem(listItem);
                    }
                } else {
                    auto listItem = new tsl::elm::ListItem(itemName);
                    bool listBackups = false;
                    std::vector<std::string> kipInfoCommand;
                    for (const std::vector<std::string>& row : commands) {
                        // Iterate over the inner vector (row)
                        for (const std::string& cell : row) {
                            if (cell == "kip_info") {
                                kipInfoCommand = row;
                                listBackups = true;
                            }
                        }
                    }
                    std::string concatenatedString;

                    // Iterate through the vector and append each string with a space
                    for (auto& str : kipInfoCommand) {
                        if (str == "{source}") {
                            str = replacePlaceholder(str, "{source}", file);
                        }
                    }
                    for (const std::string& str : kipInfoCommand) {
                        concatenatedString += str + " ";
                    }
                    if (!listBackups) {
                        listItem->setClickListener([file, this, listItem](uint64_t keys) {
                            if (keys & KEY_A) {
                                if (listItem->getValue() == "APPLIED" && !prevValue.empty()) {
                                    listItem->setValue(prevValue);
                                    prevValue = "";
                                    resetValue = false;
                                }
                                // Replace "{source}" with file in commands, then execute
                                if (!prevValue.empty()) {
                                    listItem->setValue(prevValue);
                                }
                                std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(commands, file);
                                int result = interpretAndExecuteCommand(modifiedCommands);
                                if (result == 0) {
                                    listItem->setValue("DONE", tsl::PredefinedColors::Green);
                                } else if (result == 1) {
                                    applied = true;
                                    tsl::goBack();
                                } else {
                                    listItem->setValue("FAIL", tsl::PredefinedColors::Red);
                                }
                                return true;
                            } else if (keys && (listItem->getValue() == "DONE" || listItem->getValue() == "FAIL")) {
                                listItem->setValue(prevValue);
                            }
                            return false;
                        });
                        list->addItem(listItem);
                    } else {
                        listItem->setClickListener([&, file, this, listItem, kipInfoCommand](uint64_t keys) { // Add 'command' to the capture list
                            if (keys & KEY_A) {
                                if (listItem->getValue() != "DELETED") {
                                    tsl::changeTo<KipInfoOverlay>(kipInfoCommand);
                                }
                            }
                            return false;
                        });
                        list->addItem(listItem);
                    }
                }
            } else { // for handiling toggles
                auto toggleListItem = new tsl::elm::ToggleListItem(itemName, false, "On", "Off");

                // Set the initial state of the toggle item
                bool toggleStateOn = std::find(filesListOn.begin(), filesListOn.end(), file) != filesListOn.end();
                toggleListItem->setState(toggleStateOn);

                toggleListItem->setStateChangedListener([toggleListItem, file, toggleStateOn, this](bool state) {
                    if (!state) {
                        // Toggle switched to On
                        if (toggleStateOn) {
                            std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(commands, file, true);
                            interpretAndExecuteCommand(modifiedCommands);
                        } else {
                            // Handle the case where the command should only run in the source_on section
                            // Add your specific code here
                        }
                    } else {
                        // Toggle switched to Off
                        if (!toggleStateOn) {
                            std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(commands, file, true, false);
                            interpretAndExecuteCommand(modifiedCommands);
                        } else {
                            // Handle the case where the command should only run in the source_off section
                            // Add your specific code here
                        }
                    }
                });
                list->addItem(toggleListItem);
            }
            count++;
        }
        if (hasSep) {
            if (!jsonSep.empty()) {
                list->addItem(new CustomCategoryHeader(jsonSep), 0, 0);
            }
        } else if (!useSplitHeader) {
            list->addItem(new CustomCategoryHeader(specificKey), 0, 0);
        }
        rootFrame->setContent(list);
        return rootFrame;
    }

    bool adjuct_top, adjuct_bot = false;

    virtual bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override
    {
        if (!keysDown) {
            if (this->savedItem != nullptr && this->savedItem != this->getFocusedElement()) {
                while (this->savedItem != this->getFocusedElement()) {
                    this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
                }
            } else {
                this->savedItem = nullptr;
            }
        }
        if (resetValue && keysDown) {
            if (this->getFocusedElement()->getClass() == tsl::Class::ListItem) {
                tsl::elm::ListItem* focusedItem = dynamic_cast<tsl::elm::ListItem*>(this->getFocusedElement());
                if (focusedItem->getValue() == "APPLIED") {
                    focusedItem->setValue(prevValue);
                    prevValue = "";
                    resetValue = false;
                }
            }
        }
        if (this->adjuct_top) { // Adjust cursor to the top item after jump bot-top
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            this->adjuct_top = false;
            return true;
        }
        if (this->adjuct_bot) { // Adjust cursor to the top item after jump top-bot
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            this->adjuct_bot = false;
            return true;
        }
        // Jump bot-top: scroll to the top after hitting the last list item
        if ((keysDown & KEY_DDOWN) || (keysDown & KEY_DOWN)) {
            auto prevItem = this->getFocusedElement();
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            if (prevItem == this->getFocusedElement()) {
                scrollListItems(this, ShiftFocusMode::UpMax);
                this->adjuct_top = true; // Go one item
            } else { // Adjust to account for tesla key processing
                this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            }
            return false;
        }
        // Jump bot-top: scroll to the top after hitting the last list item
        if ((keysDown & KEY_DUP) || (keysDown & KEY_UP)) {
            auto prevItem = this->getFocusedElement();
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            if (prevItem == this->getFocusedElement()) {
                scrollListItems(this, ShiftFocusMode::DownMax);
                this->adjuct_bot = true;
            } else { // Adjust to account for tesla key processing
                this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            }
            return false;
        }
        if (keysDown & KEY_L) { // Scroll to up for 5 items
            scrollListItems(this, ShiftFocusMode::UpNum);
            return true;
        }
        if (keysDown & KEY_R) { // Scroll to down for 5 items
            scrollListItems(this, ShiftFocusMode::DownNum);
            return true;
        }
        if (keysDown & KEY_ZL) { // Scroll to up for 5 items
            scrollListItems(this, ShiftFocusMode::UpMax);
            return true;
        }
        if (keysDown & KEY_ZR) { // Scroll to down for 5 items
            scrollListItems(this, ShiftFocusMode::DownMax);
            return true;
        }
        if (this->useText) {
            // process right stick scrolling
            if ((keysHeld | keysDown) & (HidNpadButton_StickRDown)) {
                this->p_list->handleInput(HidNpadButton_StickRDown, 0,{},{},{});
                return true;
            }
            if ((keysHeld | keysDown) & (HidNpadButton_StickRUp)) {
                this->p_list->handleInput(HidNpadButton_StickRUp, 0,{},{},{});
                return true;
            }
        }
        if (keysDown & KEY_B) {
            if (!Mtrun) {
                tsl::goBack();
            }
            return true;
        } else if ((applied || deleted) && !keysDown) {
            deleted = false;
            tsl::elm::ListItem* focusedItem = dynamic_cast<tsl::elm::ListItem*>(this->getFocusedElement());
            if (prevValue.empty())
                prevValue = focusedItem->getValue();
            if (applied) {
                resetValue = true;
                focusedItem->setValue("APPLIED", tsl::PredefinedColors::Green);
            } else
                focusedItem->setValue("DELETED", tsl::PredefinedColors::Red);
            applied = false;
            deleted = false;
            return true;
        }
        return false;
    }
};

class SubMenu : public tsl::Gui {
protected:
    std::string subPath;
    std::string configName = "config.ini";  // <-- Новый параметр
	std::string pathReplace;
    std::string pathReplaceOn;
    std::string pathReplaceOff;
public:
    SubMenu(const std::string& path, const std::string& config = "config.ini")
        : subPath(path), configName(config)
    {
    }
    ~SubMenu() { }

    FILE* kipFile = nullptr;
    int custOffset;

    auto addSliderItem(auto& sliderOption)
    {
        bool has_zero = false;
        std::string slider_prompt = ""; // temp insert
        const std::string& sliderName = sliderOption[1];
        const int low = std::stoi(sliderOption[2]);
        const int high = std::stoi(sliderOption[3]);
        const int step = std::stoi(sliderOption[4]);
        const std::string& offset = sliderOption[5];
        if (sliderOption.size() > 6 && sliderOption[6] == "has_zero") {
            has_zero = true;
            if (sliderOption.size() > 7) {
                slider_prompt = sliderOption[7];
            }
        } else if (sliderOption.size() > 6) {
            slider_prompt = sliderOption[6];
        }
        std::vector<std::string> myArray;
        const std::string CUST = "43555354";
        std::string header;
        int initProgress;

        if (has_zero) {
            header = sliderName + std::to_string(0);
            myArray.push_back(header);
        }

        for (int i = low; i <= high; i = i + step) {
            header = sliderName + std::to_string(i);
            myArray.push_back(header);
        }

        myArray.shrink_to_fit();
        auto slider = new tsl::elm::NamedStepTrackBar(" ",myArray,slider_prompt);

        std::string currentHex = readHexDataAtOffset("/atmosphere/kips/loader.kip", CUST, std::stoul(offset), 4);

        if (has_zero && reversedHexToInt(currentHex) == 0) {
            initProgress = 0;
        } else if (has_zero) {
            initProgress = ((reversedHexToInt(currentHex) - low)/step) + 1;
        } else {
            initProgress = (reversedHexToInt(currentHex) - low)/step;
        }

        slider->setProgress(initProgress);

        slider->setClickListener([this, slider, offset, low, step, has_zero](uint64_t keys) { // Add 'command' to the capture list
            if (keys & KEY_A) {
                int value;
                if (has_zero) {
                    if (slider->getProgressStep() == 0) {
                        value = 0;
                    } else {
                        value = low + (step * (slider->getProgressStep()-1));
                    }
                } else {
                    value = low + (step * slider->getProgressStep());
                }
                auto hexData = decimalToReversedHex(std::to_string(value));
                hexEditCustOffset("/atmosphere/kips/loader.kip", std::stoul(offset), hexData);
                slider->setColor(tsl::PredefinedColors::Green);
                return true;
            } else if (keys) {
                slider->setColor();
            }
            return false;
        });
        return slider;
    } 

    std::string findCurrentIni(const std::string& jsonPath, const std::string& iniPath, const std::string& section, const std::string& key)
    {
        std::string searchKey;

        auto jsonData = readJsonFromFile(jsonPath);
        if (jsonData && json_is_array(jsonData)) {
            size_t arraySize = json_array_size(jsonData);
            if (arraySize < 2) {
                return "\u25B6";
            }
            json_t* item = json_array_get(jsonData, 1);
            if (item && json_is_object(item)) {
                json_t* hexValue = json_object_get(item, "hex");
                json_t* decValue = json_object_get(item, "dec");
                json_t* otherValue = json_object_get(item, "value");
                if ((hexValue && json_is_string(hexValue))) {
                    searchKey = "hex";
                } else if ((decValue && json_is_string(decValue))) {
                    searchKey = "dec";
                } else if ((otherValue && json_is_string(otherValue))) {
                    searchKey = "value";
                } else {
                    return "\u25B6";
                }
                std::string iniValue = readIniValue(iniPath, section, key);
                if (!iniValue.empty()) {
                    for (size_t i = 0; i < arraySize; ++i) {
                        json_t* item = json_array_get(jsonData, i);
                        if (item && json_is_object(item)) {
                            json_t* searchItem = json_object_get(item, searchKey.c_str());
                            if (searchItem && json_is_string(searchItem)) {
                                if (json_string_value(searchItem) == iniValue) {
                                    json_t* name = json_object_get(item, "name");
                                    std::string cur_name = json_string_value(name);
                                    size_t footer_pos = cur_name.find(" - ");
                                    if (footer_pos != std::string::npos) {
                                        cur_name.resize(footer_pos);
                                    }
                                    return cur_name;
                                }
                            }
                        }
                    }
                }
            }
        }
        return "\u25B6";
    }

    virtual tsl::elm::Element* createUI() override
    {
        // log ("SubMenu");

        int numSlashes = count(subPath.begin(), subPath.end(), '/');
        bool integrityCheck = verifyIntegrity(subPath);
		if (configName == "alt_config.ini" || configName == "alt_exec.ini") {
			isAltMenu = true;
		}
        size_t fifthSlashPos = subPath.find('/', subPath.find('/', subPath.find('/', subPath.find('/') + 1) + 1) + 1);
        bool hasHelp = false;
        std::string helpPath = "";
        if (fifthSlashPos != std::string::npos) {
            // Extract the substring up to the fourth slash
            helpPath = subPath.substr(0, fifthSlashPos);
            helpPath += "/Help/" + getNameFromPath(subPath) + ".txt";
            if (isFileOrDirectory(helpPath)) {
                hasHelp = true;
            } else {
                helpPath = "";
            }
        }

        std::string viewPackage = package;
        std::string viewsubPath = getNameFromPath(subPath);

       std::string title = getNameFromPath(viewsubPath);
		if (isAltMenu) {
			title += " [Alt Mode]";
		}
		auto rootFrame = new tsl::elm::OverlayFrame(title, viewPackage, "", hasHelp);
        auto list = new tsl::elm::List();

        if (!kipVersion.empty()) {
            constexpr int lineHeight = 20; // Adjust the line height as needed
            constexpr int fontSize = 19; // Adjust the font size as needed
            const std::string CUST = "43555354";
            std::string curKipVer = readHexDataAtOffset("/atmosphere/kips/loader.kip", CUST, 4, 3);
            int i_curKipVer = reversedHexToInt(curKipVer);
            if (std::stoi(kipVersion) != i_curKipVer) {
                list->addItem(new tsl::elm::CustomDrawer([lineHeight, fontSize](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
                                  renderer->drawString("Kip version mismatch.\nUpdate the requirements to use this\npackage.", false, x, y + lineHeight, fontSize, a(tsl::style::color::ColorText));
                              }),
                              fontSize * 3 + lineHeight);
                rootFrame->setContent(list);
                return rootFrame;
            }
        }

        if (!enableConfigNav) {
            std::vector<std::string> subdirectories = getSubdirectories(subPath);
            std::sort(subdirectories.begin(), subdirectories.end());
            for (const auto& subDirectory : subdirectories) {
                if (isFileOrDirectory(subPath + subDirectory + '/' + configFileName)) {
                    auto item = new tsl::elm::ListItem(subDirectory);
                    item->setValue("\u25B6", tsl::PredefinedColors::White);
                    item->setClickListener([&, subDirectory, helpPath](u64 keys) -> bool {
                        if (keys & KEY_A) {
                            tsl::changeTo<SubMenu>(subPath + subDirectory + '/');
                            return true;
                        } else if (keys & KEY_Y && !helpPath.empty()) {
                            tsl::changeTo<HelpOverlay>(helpPath);
                            return true;
                        }
                        return false;
                    });
                    list->addItem(item);
                }
            }
        }

        // Add a section break with small text to indicate the "Commands" section
        // list->addItem(new tsl::elm::CategoryHeader("Commands"));

        // Load options from INI file in the subdirectory
        std::string subConfigIniPath = subPath + "/" + configName;
        std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>> options = loadOptionsFromIni(subConfigIniPath);

        // Package Info
        PackageHeader packageHeader = getPackageHeaderFromIni(subConfigIniPath);

        // Populate the sub menu with options
        for (const auto& option : options) {
            std::string optionName = option.first;
            std::string footer;
            bool usePattern = false;
            bool useSlider = false;
            bool useSliderItem = false;
            std::string toggleMode;
            std::string toggleVerPath;
            std::string toggleVerLine;
            std::string headerName;
            if (optionName[0] == '@') { // a subdirectory. add a menu item and skip to the next command
                std::vector<std::string> tmpldir = option.second[0];
                auto item = new tsl::elm::ListItem(optionName.substr(1));
                item->setValue("\u25B6", tsl::PredefinedColors::White);
                item->setClickListener([&, tmpldir, item](u64 keys) -> bool {
                    if (keys & KEY_A) {
                        if (!isFileOrDirectory(tmpldir[1])) {
                            item->setValue("FAIL", tsl::PredefinedColors::Red);
                            return true;
                        }
                        tsl::changeTo<KipInfoOverlay>(tmpldir, false);
                        return true;
                    }
                    return false;
                });
                list->addItem(item);
                continue;
            }

			if (enableConfigNav && optionName[0] == '>') {
				std::string subDirectory = optionName.substr(1);

				auto item = new tsl::elm::ListItem(subDirectory);
				item->setValue("\u25B6", tsl::PredefinedColors::White);  // Стандартный символ (треугольник)

				item->setClickListener([&, subDirectory, item](u64 keys) -> bool {
					std::string normalPath = subPath + subDirectory + "/";
					std::string altPath = normalPath + "alt_config.ini";

					if (keys & KEY_A) {
						if (!isDirectory(normalPath)) {
							item->setValue("\u25B6", tsl::PredefinedColors::White);
							return true;
						}
						isAltMenu = false;  // Переход в обычное меню
						tsl::changeTo<SubMenu>(normalPath);
						return true;
					} else if (keys & KEY_RSTICK) {
						if (!isFileOrDirectory(altPath)) {
							item->setValue("\u25B6", tsl::PredefinedColors::White);
							return true;
						}
						isAltMenu = true;  // Переход в альтернативное меню
						tsl::changeTo<SubMenu>(normalPath, "alt_config.ini");
						return true;
					}
					return false;
				});

				list->addItem(item);
				continue;
            } else if (optionName[0] == '*') {
                if (option.second[0][0] == "slider_kip") {
                    useSliderItem = true;
                    footer = "";
                } else {
                    usePattern = true;
                    footer = "\u25B6";
                }
                optionName = optionName.substr(1); // Strip the "-" character on the left
            } else if (optionName[0] == '-') {
                useSlider = true;
                optionName = optionName.substr(1); // Strip the "-" character on the left
                footer = "\u25B6";
            } else {
                size_t pos = optionName.find(" - ");
                if (pos != std::string::npos) {
                    footer = optionName.substr(pos + 2); // Assign the part after "&&" as the footer
                    optionName.resize(pos); // Strip the "&&" and everything after it
                }
            }

            size_t pos = optionName.find(" ;; "); // Find the custom display header
            if (pos != std::string::npos) {
                headerName = optionName.substr(pos + 4); // Strip the item name
                optionName.resize(pos); // Strip the displayName
            } else {
                headerName = optionName;
            }

            // Extract the path pattern from commands
            bool useToggle = false;
            bool useAdvToggle = false;
            bool isSeparator = false;
            for (const auto& cmd : option.second) {
                if (cmd[0] == "separator") {
                    isSeparator = true;
                    break;
                }
                if (cmd.size() > 1) {
                    if (cmd[0] == "source") {
                        pathReplace = cmd[1];
                    } else if (cmd[0] == "source_on") {
                        pathReplaceOn = cmd[1];
                        useToggle = true;
                    } else if (cmd[0] == "source_off") {
                        pathReplaceOff = cmd[1];
                        useToggle = true;
                    } else if (cmd[0] == "toggle_state") {
                        toggleMode = cmd[1];
                        toggleVerPath = cmd[2];
                        if (cmd.size() > 3) {
                            toggleVerLine = cmd[3];
                        }
                        useAdvToggle = true;
                    }
                }
            }

            if (isSeparator) {
                auto item = new CustomCategoryHeader(optionName, true);
                list->addItem(item);
            } else if (useSliderItem) {
                auto item = addSliderItem(option.second[0]);
                list->addItem(item);
            } else if (useAdvToggle) {
                bool toggleStateOn = false;
                if (toggleMode == "file_exists") {
                    toggleStateOn = isFileOrDirectory(toggleVerPath);
                } else if (toggleMode == "has_line") {
                    toggleStateOn = isLineExistInIni(toggleVerPath, toggleVerLine);
                }
                auto toggleListItem = new tsl::elm::ToggleListItem(optionName, toggleStateOn, "On", "Off");
                std::vector<std::vector<std::string>> toggleOn;
                std::vector<std::vector<std::string>> toggleOff;

                for (auto command : option.second) {
                    if (command[0] == "toggle_on") {
                        toggleOn.push_back(std::vector<std::string>(command.begin() + 1, command.end()));
                    } else if (command[0] == "toggle_off") {
                        toggleOff.push_back(std::vector<std::string>(command.begin() + 1, command.end()));
                    }
                }

                toggleListItem->setStateChangedListener([toggleListItem, toggleOn, toggleOff, toggleStateOn, this](bool state) {
                    if (state) {
                        std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(toggleOn, "", false, true, true);
                        interpretAndExecuteCommand(modifiedCommands);
                    } else {
                        std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(toggleOff, "", false, true, true);
                        interpretAndExecuteCommand(modifiedCommands);
                    }
                });
                list->addItem(toggleListItem);
            } else if (usePattern || !useToggle || useSlider) {
                auto listItem = static_cast<tsl::elm::ListItem*>(nullptr);
                if ((footer == "\u25B6") || (footer.empty())) {
                    listItem = new tsl::elm::ListItem(optionName, footer);
                } else {
                    listItem = new tsl::elm::ListItem(optionName);
                    listItem->setValue(footer);
                }
                for (const auto& cmd : option.second) {
                    if ((cmd[0] == "json_mark_cur_kip") && showCurInMenu) {
                        if (!kipFile) {
                            kipFile = openFile("/atmosphere/kips/loader.kip");
                            custOffset = findCustOffset(kipFile);
                        }
                        auto& offset = cmd[3];
                        std::string jsonPath = preprocessPath(cmd[1]);
                        listItem->setValue(findCurrentKip(jsonPath, offset, kipFile, custOffset));
                    }
                    if ((cmd[0] == "json_mark_cur_ini") && showCurInMenu) {
                        std::string sourceIni = preprocessPath(cmd[3]);
                        auto& sectionIni = cmd[4];
                        auto& keyIni = cmd[5];
                        std::string jsonPath = preprocessPath(cmd[1]);
                        listItem->setValue(findCurrentIni(jsonPath, sourceIni, sectionIni, keyIni));
                    }
                }

            listItem->setClickListener([&, command = option.second, keyName = headerName, subPath = this->subPath, usePattern, listItem, helpPath, useSlider, footer](uint64_t keys) {

					if (exitMT) {
						threadClose(&threadMT);
						exitMT = false;
						Mtrun = false;

						if (errCode == 1) {
							tsl::goBack();
							return true;
						}
					}

					if (!Mtrun) {
						std::vector<std::vector<std::string>> selectedCommand;

						// 🔹 Если нажата кнопка RStick — загрузить команду из alt_exec.ini
						if (keys & KEY_RSTICK) {
							std::string altExecPath = subPath + "/" + "alt_exec.ini";

							if (isFileOrDirectory(altExecPath)) {
								auto altOptions = loadOptionsFromIni(altExecPath);
								for (const auto& altOption : altOptions) {
									if (altOption.first == "*" + keyName) {
										selectedCommand = altOption.second;

										// Убираем пометку Alt Mode и не добавляем её в listItem
										// isAltMenu = true; // Больше не используем флаг для Alt Menu

										break;
									}
								}
							}
						}

						// 🔸 Если нажата кнопка A — использовать стандартную команду
						else if (keys & KEY_A) {
							selectedCommand = command;
						}

						if (!selectedCommand.empty()) {
							if (listItem->getValue() == "APPLIED" && !prevValue.empty()) {
								listItem->setValue(prevValue);
								prevValue = "";
								resetValue = false;
							}

							if (usePattern) {
								tsl::changeTo<SelectionOverlay>(subPath, keyName, selectedCommand);
							} else if (useSlider) {
								tsl::changeTo<FanSliderOverlay>(subPath, keyName, selectedCommand);
							} else {
								ThreadArgs args;
								args.exitMT = &exitMT;
								args.commands = selectedCommand;
								args.listItem = listItem;
								args.errCode = &errCode;
								args.progress = "temp";

								Result rc = threadCreate(&threadMT, MTinterpretAndExecute, &args, NULL, 0x10000, 0x2C, -2);
								if (R_FAILED(rc)) log("error in thread create");

								Result rcs = threadStart(&threadMT);
								if (R_FAILED(rcs)) log("error in thread start");

								Mtrun = true;
							}
							return true;
						}
					}

					// Остальные кнопки (X, Y и восстановление состояния)
					if (keys & KEY_X) {
						tsl::changeTo<ConfigOverlay>(subPath, keyName);
						return true;
					} else if (keys & KEY_Y && !helpPath.empty()) {
						tsl::changeTo<HelpOverlay>(helpPath);
					} else if (keys && (listItem->getValue() == "DONE" || listItem->getValue() == "FAIL")) {
						listItem->setValue(footer);
					}

					return false;
				});


                list->addItem(listItem);
            } else {
                auto toggleListItem = new tsl::elm::ToggleListItem(optionName, false, "On", "Off");
                // Set the initial state of the toggle item
                bool toggleStateOn = isFileOrDirectory(preprocessPath(pathReplaceOn));

                toggleListItem->setState(toggleStateOn);

                toggleListItem->setStateChangedListener([toggleStateOn, command = option.second, this](bool state) {
                    if (!state) {
                        // Toggle switched to On
                        if (toggleStateOn) {
                            std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(command, pathReplaceOn, true);
                            interpretAndExecuteCommand(modifiedCommands);
                        } else {
                            // Handle the case where the command should only run in the source_on section
                            // Add your specific code here
                        }
                    } else {
                        // Toggle switched to Off
                        if (!toggleStateOn) {
                            std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(command, pathReplaceOff, true, false);
                            interpretAndExecuteCommand(modifiedCommands);
                        } else {
                            // Handle the case where the command should only run in the source_off section
                            // Add your specific code here
                        }
                    }
                });

                list->addItem(toggleListItem);
            }
        }

        if (kipFile) {
            fclose(kipFile);
            kipFile = nullptr;
        }
        constexpr int lineHeight = 20; // Adjust the line height as needed
        constexpr int xOffset = 120; // Adjust the horizontal offset as needed
        constexpr int fontSize = 16; // Adjust the font size as needed
        int numEntries = 0; // Adjust the number of entries as needed

        std::string packageSectionString = "";
        std::string packageInfoString = "";
        if (packageHeader.version != "") {
            packageSectionString += "Version\n";
            packageInfoString += (packageHeader.version + "\n").c_str();
            numEntries++;
        }
        if (packageHeader.creator != "") {
            packageSectionString += "Creator(s)\n";
            packageInfoString += (packageHeader.creator + "\n").c_str();
            numEntries++;
        }
        if (packageHeader.about != "") {
            std::string aboutHeaderText = "About\n";
            std::string::size_type aboutHeaderLength = aboutHeaderText.length();
            std::string aboutText = packageHeader.about;

            packageSectionString += aboutHeaderText;

            // Split the about text into multiple lines with proper word wrapping
            constexpr int maxLineLength = 28; // Adjust the maximum line length as needed
            std::string::size_type startPos = 0;
            std::string::size_type spacePos = 0;

            while (startPos < aboutText.length()) {
                std::string::size_type endPos = std::min(startPos + maxLineLength, aboutText.length());
                std::string line = aboutText.substr(startPos, endPos - startPos);

                // Check if the current line ends with a space; if not, find the last space in the line
                if (endPos < aboutText.length() && aboutText[endPos] != ' ') {
                    spacePos = line.find_last_of(' ');
                    if (spacePos != std::string::npos) {
                        endPos = startPos + spacePos;
                        line = aboutText.substr(startPos, endPos - startPos);
                    }
                }

                packageInfoString += line + '\n';
                startPos = endPos + 1;
                numEntries++;

                // Add corresponding newline to the packageSectionString
                if (startPos < aboutText.length()) {
                    packageSectionString += std::string(aboutHeaderLength, ' ') + '\n';
                }
            }
        }

        // Remove trailing newline character
        if ((packageSectionString != "") && (packageSectionString.back() == '\n')) {
            packageSectionString.resize(packageSectionString.size() - 1);
        }
        if ((packageInfoString != "") && (packageInfoString.back() == '\n')) {
            packageInfoString.resize(packageInfoString.size() - 1);
        }

        if ((packageSectionString != "") && (packageInfoString != "")) {
            list->addItem(new CustomCategoryHeader("Package Info"));
            list->addItem(new tsl::elm::CustomDrawer([lineHeight, xOffset, fontSize, packageSectionString, packageInfoString](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
                              renderer->drawString(packageSectionString.c_str(), false, x, y + lineHeight, fontSize, a(tsl::style::color::ColorText));
                              renderer->drawString(packageInfoString.c_str(), false, x + xOffset, y + lineHeight, fontSize, a(tsl::style::color::ColorText));
                          }),
                          fontSize * numEntries + lineHeight);
        }

        if (numSlashes == 5 && integrityCheck) {
            int headerCh[] = { 83, 112, 101, 99, 105, 97, 108, 32, 84, 104, 97, 110, 107, 115 };
            int length = sizeof(headerCh) / sizeof(headerCh[0]);
            char* charArray = new char[length + 1];
            for (int i = 0; i < length; ++i) {
                charArray[i] = static_cast<char>(headerCh[i]);
            }
            charArray[length] = '\0';
            list->addItem(new CustomCategoryHeader(charArray));
            int checksum[] = { 83, 112, 101, 99, 105, 97, 108, 32, 116, 104, 97, 110, 107, 115, 32, 116, 111, 32, 69, 102, 111, 115, 97, 109, 97, 114, 107, 44, 32, 73, 114, 110, 101, 32, 97, 110, 100, 32, 73, 51, 115, 101, 121, 46, 10, 87, 105, 116, 104, 111, 117, 116, 32, 116, 104, 101, 109, 32, 116, 104, 105, 115, 32, 119, 111, 117, 108, 100, 110, 39, 116, 32, 98, 101, 32, 112, 111, 115, 115, 98, 108, 101, 33 };
            length = sizeof(checksum) / sizeof(checksum[0]);
            charArray = new char[length + 1];
            for (int i = 0; i < length; ++i) {
                charArray[i] = static_cast<char>(checksum[i]);
            }
            charArray[length] = '\0';
            list->addItem(new tsl::elm::CustomDrawer([lineHeight, xOffset, fontSize, charArray](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
                              renderer->drawString(charArray, false, x, y + lineHeight, fontSize, a(tsl::style::color::ColorText));
                          }),
                          fontSize * 2 + lineHeight);
        }

        rootFrame->setContent(list);

        return rootFrame;
    }

    bool adjuct_top, adjuct_bot = false;

    virtual bool handleInput(uint64_t keysDown, uint64_t keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override
    {
        if (resetValue && keysDown) {
            if (this->getFocusedElement()->getClass() == tsl::Class::ListItem) {
                tsl::elm::ListItem* focusedItem = dynamic_cast<tsl::elm::ListItem*>(this->getFocusedElement());
                if (focusedItem->getValue() == "APPLIED") {
                    focusedItem->setValue(prevValue);
                    prevValue = "";
                    resetValue = false;
                }
            }
        }
        if (this->adjuct_top) { // Adjust cursor to the top item after jump bot-top
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            this->adjuct_top = false;
            return true;
        }
        if (this->adjuct_bot) { // Adjust cursor to the top item after jump top-bot
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            this->adjuct_bot = false;
            return true;
        }
        // Jump bot-top: scroll to the top after hitting the last list item
        if ((keysDown & KEY_DDOWN) || (keysDown & KEY_DOWN)) {
            auto prevItem = this->getFocusedElement();
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            if (prevItem == this->getFocusedElement()) {
                scrollListItems(this, ShiftFocusMode::UpMax);
                this->adjuct_top = true; // Go one item
            } else { // Adjust to account for tesla key processing
                this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            }
            return false;
        }
        // Jump bot-top: scroll to the top after hitting the last list item
        if ((keysDown & KEY_DUP) || (keysDown & KEY_UP)) {
            auto prevItem = this->getFocusedElement();
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            if (prevItem == this->getFocusedElement()) {
                scrollListItems(this, ShiftFocusMode::DownMax);
                this->adjuct_bot = true;
            } else { // Adjust to account for tesla key processing
                this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            }
            return false;
        }
        if (keysDown & KEY_L) { // Scroll to up for 5 items
            scrollListItems(this, ShiftFocusMode::UpNum);
            return true;
        }
        if (keysDown & KEY_R) { // Scroll to down for 5 items
            scrollListItems(this, ShiftFocusMode::DownNum);
            return true;
        }
        if (keysDown & KEY_ZL) { // Scroll to up for 5 items
            scrollListItems(this, ShiftFocusMode::UpMax);
            return true;
        }
        if (keysDown & KEY_ZR) { // Scroll to down for 5 items
            scrollListItems(this, ShiftFocusMode::DownMax);
            return true;
        }
        if (keysDown & KEY_B) {
            if (!Mtrun) {
                tsl::goBack();
            }
            return true;
        } else if (applied && !keysDown) {
            applied = false;
            resetValue = true;
            tsl::elm::ListItem* focusedItem = dynamic_cast<tsl::elm::ListItem*>(this->getFocusedElement());
            if (prevValue.empty())
                prevValue = focusedItem->getValue();
            focusedItem->setValue("APPLIED", tsl::PredefinedColors::Green);
            return true;
        }
        return false;
    }
};

class MainMenu;

class Package : public SubMenu {
public:
    Package(const std::string& path)
        : SubMenu(path)
    {
    }

    tsl::elm::Element* createUI() override
    {
        package = getNameFromPath(subPath);
        std::string subConfigIniPath = subPath + "/" + configFileName;
        PackageHeader packageHeader = getPackageHeaderFromIni(subConfigIniPath);
        enableConfigNav = packageHeader.enableConfigNav;
        showCurInMenu = packageHeader.showCurInMenu;
        kipVersion = packageHeader.checkKipVersion;

        auto rootFrame = static_cast<tsl::elm::OverlayFrame*>(SubMenu::createUI());
        rootFrame->setTitle(package);
        rootFrame->setSubtitle("                             "); // FIXME: former subtitle is not fully erased if new string is shorter
        return rootFrame;
    }

    bool adjuct_top, adjuct_bot = false;

    bool handleInput(uint64_t keysDown, uint64_t keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override
    {
        if (this->adjuct_top) { // Adjust cursor to the top item after jump bot-top
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            this->adjuct_top = false;
            return true;
        }
        if (this->adjuct_bot) { // Adjust cursor to the top item after jump top-bot
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            this->adjuct_bot = false;
            return true;
        }
        // Jump bot-top: scroll to the top after hitting the last list item
        if ((keysDown & KEY_DDOWN) || (keysDown & KEY_DOWN)) {
            auto prevItem = this->getFocusedElement();
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            if (prevItem == this->getFocusedElement()) {
                scrollListItems(this, ShiftFocusMode::UpMax);
                this->adjuct_top = true; // Go one item
            } else { // Adjust to account for tesla key processing
                this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            }
            return false;
        }
        // Jump bot-top: scroll to the top after hitting the last list item
        if ((keysDown & KEY_DUP) || (keysDown & KEY_UP)) {
            auto prevItem = this->getFocusedElement();
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            if (prevItem == this->getFocusedElement()) {
                scrollListItems(this, ShiftFocusMode::DownMax);
                this->adjuct_bot = true;
            } else { // Adjust to account for tesla key processing
                this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            }
            return false;
        }
        if (keysDown & KEY_L) { // Scroll to up for 5 items
            scrollListItems(this, ShiftFocusMode::UpNum);
            return true;
        }
        if (keysDown & KEY_R) { // Scroll to down for 5 items
            scrollListItems(this, ShiftFocusMode::DownNum);
            return true;
        }
        if (keysDown & KEY_ZL) { // Scroll to up for 5 items
            scrollListItems(this, ShiftFocusMode::UpMax);
            return true;
        }
        if (keysDown & KEY_ZR) { // Scroll to down for 5 items
            scrollListItems(this, ShiftFocusMode::DownMax);
            return true;
        }
        if (keysDown & KEY_B) {
            tsl::resetWith<MainMenu>(Packages);
            return true;
        }
        return false;
    }
};

class Updater : public tsl::Gui {
protected:
    std::vector<std::map<std::string, std::string>> uitems;
    Screen prevMenu;

public:
    Updater(const std::vector<std::map<std::string, std::string>>& items, Screen mode = Overlays)
        : uitems(items)
        , prevMenu(mode)
    {
    }

    tsl::elm::Element* createUI() override
    {
        auto rootFrame = new tsl::elm::OverlayFrame("Updates available", "Updater");
        auto list = new tsl::elm::List();

        for (const auto& item : uitems) {
            auto* listItem = new tsl::elm::ListItem(item.at("name"), item.at("repoVer"));
            listItem->setClickListener([item, listItem](s64 key) {
                if (key & KEY_A) {
                    if (!DownloadProcessing) {
                        DownloadProcessing = true;
                        listItem->setText("Processing...");
                        return true;
                    }
                }
                if (DownloadProcessing) {
                    std::string type = item.at("type");
                    std::string link = item.at("link");
                    std::string name = item.at("name");
                    std::string fullPath = item.at("filename");

                    if (type == "ovl") {
                        if (!moveFileOrDirectory(fullPath, fullPath + "_old")) {
                            listItem->setValue("FAIL", tsl::PredefinedColors::Red);
                            log("Update: Error during rename of %s", fullPath.c_str());
                            DownloadProcessing = false;
                            return false;
                        }
                        if (downloadFile(link, "sdmc:/switch/.overlays/")) {
                            listItem->setText(name);
                            listItem->setValue("DONE", tsl::PredefinedColors::Green);
                            DownloadProcessing = false;
                            deleteFileOrDirectory(fullPath + "_old");
                            return true;
                        } else {
                            log("Update: Error during download");
                            if (!moveFileOrDirectory(fullPath + "_old", fullPath)) {
                                log("Update: Rollback failed");
                            }
                        }
                    } else if (type == "pkgzip") {
                        std::string tempZipPath = packageDirectory + "temp.zip";

                        if (!moveFileOrDirectory(fullPath, fullPath + "_old")) {
                            listItem->setValue("FAIL", tsl::PredefinedColors::Red);
                            log("Update: Error during rename of %s", fullPath.c_str());
                            DownloadProcessing = false;
                            return false;
                        }
                        if (downloadFile(link, tempZipPath) && unzipFile(tempZipPath, packageDirectory)) {
                            deleteFileOrDirectory(tempZipPath);
                            listItem->setText(name);
                            listItem->setValue("DONE", tsl::PredefinedColors::Green);
                            deleteFileOrDirectory(fullPath + "_old");
                            DownloadProcessing = false;
                            return true;
                        } else {
                            log("Update: Error during download");
                            if (!moveFileOrDirectory(fullPath + "_old", fullPath)) {
                                log("Update: Rollback failed");
                            }
                        }
                    } else if (type == "ovlzip") {
                        std::string tempZipPath = packageDirectory + "temp.zip";
                        std::string destFolderPath = "sdmc:/";

                        if (downloadFile(link, tempZipPath) && unzipFile(tempZipPath, destFolderPath)) {
                            deleteFileOrDirectory(tempZipPath);
                            listItem->setText(name);
                            listItem->setValue("DONE", tsl::PredefinedColors::Green);
                            DownloadProcessing = false;
                            return true;
                        }
                    }

                    listItem->setValue("FAIL", tsl::PredefinedColors::Red);
                    DownloadProcessing = false;
                }
                return false;
            });

            list->addItem(listItem);
        }

        rootFrame->setContent(list);
        return rootFrame;
    }

    bool adjuct_top, adjuct_bot = false;

    bool handleInput(uint64_t keysDown, uint64_t keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override
    {
        if (this->adjuct_top) { // Adjust cursor to the top item after jump bot-top
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            this->adjuct_top = false;
            return true;
        }
        if (this->adjuct_bot) { // Adjust cursor to the top item after jump top-bot
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            this->adjuct_bot = false;
            return true;
        }
        // Jump bot-top: scroll to the top after hitting the last list item
        if ((keysDown & KEY_DDOWN) || (keysDown & KEY_DOWN)) {
            auto prevItem = this->getFocusedElement();
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            if (prevItem == this->getFocusedElement()) {
                scrollListItems(this, ShiftFocusMode::UpMax);
                this->adjuct_top = true; // Go one item
            } else { // Adjust to account for tesla key processing
                this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            }
            return false;
        }
        // Jump bot-top: scroll to the top after hitting the last list item
        if ((keysDown & KEY_DUP) || (keysDown & KEY_UP)) {
            auto prevItem = this->getFocusedElement();
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            if (prevItem == this->getFocusedElement()) {
                scrollListItems(this, ShiftFocusMode::DownMax);
                this->adjuct_bot = true;
            } else { // Adjust to account for tesla key processing
                this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            }
            return false;
        }
        if (keysDown & KEY_L) { // Scroll to up for 5 items
            scrollListItems(this, ShiftFocusMode::UpNum);
            return true;
        }
        if (keysDown & KEY_R) { // Scroll to down for 5 items
            scrollListItems(this, ShiftFocusMode::DownNum);
            return true;
        }
        if (keysDown & KEY_ZL) { // Scroll to up for 5 items
            scrollListItems(this, ShiftFocusMode::UpMax);
            return true;
        }
        if (keysDown & KEY_ZR) { // Scroll to down for 5 items
            scrollListItems(this, ShiftFocusMode::DownMax);
            return true;
        }
        if (keysDown & KEY_B) {
            tsl::resetWith<MainMenu>(this->prevMenu);
            return true;
        }

        return false;
    }
};

class MainMenu : public tsl::Gui {
private:
    tsl::hlp::ini::IniData settingsData;
    std::string packageConfigIniPath = packageDirectory + configFileName;
    std::string menuMode, defaultMenuMode, inOverlayString, fullPath, optionName, repoUrl;
    int priority;
    bool useDefaultMenu = false, showOverlayVersions = false, showPackageVersions = false;
    bool uberhand_updates_only = false;
    bool packageUpdater = true;
    bool overlayUpdater = true;
    bool sorting = false;
    Screen ForMode;

public:
    MainMenu(const Screen& curScreen = Default)
        : ForMode(curScreen)
    {
    }
    ~MainMenu() { }

    virtual tsl::elm::Element* createUI() override
    {
        // log ("MainMenu");

        defaultMenuMode = "overlays";
        menuMode = "overlays";

        createDirectory(packageDirectory);
        createDirectory(settingsPath);

        switch (ForMode) {
        case Soverlays:
            sorting = true;
            menuMode = "overlays";
            break;
        case Spackages:
            sorting = true;
            menuMode = "packages";
            break;
        case Overlays:
            sorting = false;
            menuMode = "overlays";
            break;
        case Packages:
            sorting = false;
            menuMode = "packages";
            break;
        case Default:
            break;
        }

        bool settingsLoaded = false;
        if (isFileOrDirectory(settingsConfigIniPath)) {
            settingsData = getParsedDataFromIniFile(settingsConfigIniPath);
            if (settingsData.count("uberhand") > 0) {
                auto& uberhandSection = settingsData["uberhand"];
                if (uberhandSection.count("last_menu") > 0) {
                    if (uberhandSection.count("default_menu") > 0) {
                        defaultMenuMode = uberhandSection["default_menu"];
                        if (uberhandSection.count("in_overlay") > 0) {
                            settingsLoaded = true;
                        }
                    }
                    if (defaultMenuMode == "last_menu") {
                        menuMode = uberhandSection["last_menu"];
                    }
                }
                if (uberhandSection["show_ovl_versions"] == "true") {
                    showOverlayVersions = true;
                } else {
                    setIniFileValue(settingsConfigIniPath, "uberhand", "show_ovl_versions", "false");
                }
                if (uberhandSection["show_pack_versions"] == "true") {
                    showPackageVersions = true;
                } else {
                    setIniFileValue(settingsConfigIniPath, "uberhand", "show_pack_versions", "false");
                }
                if (uberhandSection["uberhand_updates_only"] == "true") {
                    uberhand_updates_only = true;
                } else {
                    setIniFileValue(settingsConfigIniPath, "uberhand", "uberhand_updates_only", "false");
                }
                if (!uberhandSection["ovl_repo"].empty()) {
                    repoUrl = uberhandSection["ovl_repo"];
                } else {
                    repoUrl = "https://raw.githubusercontent.com/i3sey/uUpdater-ovl-repo/main/main.ini";
                    setIniFileValue(settingsConfigIniPath, "uberhand", "ovl_repo", repoUrl);
                }
                if (uberhandSection["package_updater"] == "false") {
                    packageUpdater = false;
                } else {
                    setIniFileValue(settingsConfigIniPath, "uberhand", "package_updater", "true");
                }
                if (uberhandSection["overlay_updater"] == "false") {
                    overlayUpdater = false;
                } else {
                    setIniFileValue(settingsConfigIniPath, "uberhand", "overlay_updater", "true");
                }
                if (!(uberhandSection["show_separator"] == "true")) {
                    setIniFileValue(settingsConfigIniPath, "uberhand", "show_separator", "false");
                }
                if (uberhandSection["key_combo"].empty()) {
                    copyTeslaKeyComboTouberhand();
                } else {
                    if (!isDirectory("sdmc:/config/tesla/")) {
                        createDirectory("sdmc:/config/tesla/");
                        setIniFileValue(teslaSettingsConfigIniPath, "tesla", "key_combo", uberhandSection["key_combo"]);
                    } else if (!sameKeyCombo) {
                        std::string teslaCombo = readIniValue(teslaSettingsConfigIniPath, "tesla", "key_combo");
                        if (teslaCombo != uberhandSection["key_combo"]) {
                            sameKeyCombo = true;
                            setIniFileValue(teslaSettingsConfigIniPath, "tesla", "key_combo", uberhandSection["key_combo"]);
                        } else {
                            sameKeyCombo = true;
                        }
                    }
                }
            }
        }

        if (!isFileOrDirectory("sdmc:/config/tesla/config.ini")) // Create and modify tesla kinfog

            if (!settingsLoaded) { // write data if settings are not loaded
                setIniFileValue(settingsConfigIniPath, "uberhand", "default_menu", defaultMenuMode);
                setIniFileValue(settingsConfigIniPath, "uberhand", "last_menu", menuMode);
                setIniFileValue(settingsConfigIniPath, "uberhand", "in_overlay", "false");
            }
        //setIniFileValue(settingsConfigIniPath, "uberhand", "in_overlay", "false");

        if ((defaultMenuMode == "overlays") || (defaultMenuMode == "packages")) {
            if (defaultMenuLoaded) {
                menuMode = defaultMenuMode;
                defaultMenuLoaded = false;
            }
        } else {
            defaultMenuMode = "last_menu";
            setIniFileValue(settingsConfigIniPath, "uberhand", "default_menu", defaultMenuMode);
        }

        auto rootFrame = new tsl::elm::OverlayFrame("Uberhand", APP_VERSION, menuMode);
        auto list = new tsl::elm::List();

        //loadOverlayFiles(list);

        int count = 0;

        if (menuMode == "overlays") {
            // Load overlay files
            std::vector<std::string> overlayFiles = getFilesListByWildcard(overlayDirectory + "*.ovl");

            FILE* overlaysIniFile = fopen(overlaysIniFilePath.c_str(), "r");
            if (!overlaysIniFile) {
                fclose(fopen(overlaysIniFilePath.c_str(), "w")); // The INI file doesn't exist, so create an empty one.
            } else {
                fclose(overlaysIniFile); // The file exists, so close it.
            }
            //std::sort(overlayFiles.begin(), overlayFiles.end()); // Sort overlay files alphabetically

            std::string overlayFileName;

            if (!overlayFiles.empty()) {
                std::map<std::string, std::map<std::string, std::string>> overlaysIniData = getParsedDataFromIniFile(overlaysIniFilePath);
                std::multimap<int, std::string> order;
                std::map<std::string, std::string> alpSort;

                for (const std::string& sortElem : overlayFiles) {
                    auto [result, overlayName, overlayVersion] = getOverlayInfo(sortElem);
                    if (result != ResultSuccess) {
                        continue;
                    }
                    if (overlayName == "Uberhand") {
                        continue;
                    }
                    alpSort[overlayName] = sortElem;
                }
                overlayFiles.clear();
                for (const auto& overlay : alpSort) {
                    overlayFiles.push_back(overlay.second);
                }

                for (const auto& overlayFile : overlayFiles) {
                    overlayFileName = getNameFromPath(overlayFile);
                    int priority = 0;

                    if (getNameFromPath(overlayFile) == "ovlmenu.ovl") {
                        continue;
                    }

                    if (overlaysIniData.find(overlayFileName) == overlaysIniData.end()) {
                        setIniFileValue(overlaysIniFilePath, overlayFileName, "priority", "0");
                    } else {
                        // Check if the "priority" key exists in overlaysIniData for overlayFileName
                        if (overlaysIniData.find(overlayFileName) != overlaysIniData.end() && overlaysIniData[overlayFileName].find("priority") != overlaysIniData[overlayFileName].end()) {
                            priority = -(stoi(overlaysIniData[overlayFileName]["priority"]));
                        } else
                            setIniFileValue(overlaysIniFilePath, overlayFileName, "priority", "0");
                    }
                    order.emplace(priority, overlayFileName);
                }

                for (const auto& overlay : order) {
                    overlayFileName = overlay.second;

                    auto [result, overlayName, overlayVersion] = getOverlayInfo(overlayDirectory + overlayFileName);
                    if (result != ResultSuccess)
                        continue;

                    auto* listItem = new tsl::elm::ListItem(overlayName);
                    if (showOverlayVersions)
                        listItem->setValue(overlayVersion);

                    if (sorting) {
                        std::map<std::string, std::map<std::string, std::string>> overlaysIniData = getParsedDataFromIniFile(overlaysIniFilePath);
                        priority = std::stoi(overlaysIniData[overlayFileName]["priority"]);
                        listItem->setValue("Priority: " + std::to_string(priority));
                    }
                    // Add a click listener to load the overlay when clicked upon
                    listItem->setClickListener([this, overlayFile = overlayDirectory + overlayFileName, listItem, overlayFileName](s64 key) {
                        int localPriority;
                        if (key & KEY_A) {
                            // Load the overlay here
                            setIniFileValue(settingsConfigIniPath, "uberhand", "in_overlay", "true"); // this is handled within tesla.hpp
                            tsl::setNextOverlay(overlayFile);
                            tsl::Overlay::get()->close();
                            return true;
                        } else if (key & KEY_PLUS) {
                            std::map<std::string, std::map<std::string, std::string>> overlaysIniData = getParsedDataFromIniFile(overlaysIniFilePath);
                            localPriority = std::stoi(overlaysIniData[overlayFileName]["priority"]) + 1;
                            setIniFileValue(overlaysIniFilePath, overlayFileName, "priority", std::to_string(localPriority));
                            tsl::replaceWith<MainMenu>(Soverlays);
                            return true;
                        } else if (key & KEY_MINUS) {
                            std::map<std::string, std::map<std::string, std::string>> overlaysIniData = getParsedDataFromIniFile(overlaysIniFilePath);
                            localPriority = std::stoi(overlaysIniData[overlayFileName]["priority"]) - 1;
                            setIniFileValue(overlaysIniFilePath, overlayFileName, "priority", std::to_string(localPriority));
                            tsl::replaceWith<MainMenu>(Soverlays);
                            return true;
                        }
                        return false;
                    });

                    if (count == 0) {
                        list->addItem(new CustomCategoryHeader("Overlays"));
                    }
                    list->addItem(listItem);
                    count++;
                }
            }

            //ovl updater section
            if (overlayUpdater) {
                list->addItem(new CustomCategoryHeader("Updater", true));
                auto updaterItem = new tsl::elm::ListItem("Check for Overlay Updates");
                updaterItem->setClickListener([this, updaterItem](uint64_t keys) {
                    if (keys & KEY_A) {
                        if (!DownloadProcessing) {
                            DownloadProcessing = true;
                            updaterItem->setText("Processing...");
                            return true;
                        }
                    }
                    if (DownloadProcessing) {
                        bool NeedUpdate = false;
                        std::vector<std::map<std::string, std::string>> items;
                        if (!uberhand_updates_only) {
                            std::vector<std::string> overlays = getFilesListByWildcard("sdmc:/switch/.overlays/*.ovl");
                            std::map<std::string, std::string> package;
                            if (downloadFile(repoUrl, "sdmc:/config/uberhand/Updater.ini")) {
                                auto options = loadOptionsFromIni("sdmc:/config/uberhand/Updater.ini");
                                for (const std::string& overlay : overlays) {
                                    std::string uoverlay = dropExtension(getNameFromPath(overlay));
                                    for (const auto& [name, parameters] : options) {
                                        if (uoverlay == name) {
                                            auto [result, overlayName, overlayVersion] = getOverlayInfo(overlay);
                                            if (result != ResultSuccess)
                                                continue;
                                            package["name"] = overlayName;
                                            package["filename"] = overlay;
                                            if (parameters.size() > 1 && parameters[1].size() > 0 && parameters[1][0].starts_with("link=")) {
                                                package["link"] = parameters[1][0].substr(5); // skip "link="
                                            } else {
                                                log("Overlay Updater:ERROR: link not found for item \"%s\"", overlayName.c_str());
                                                break;
                                            }
                                            package["localVer"] = overlayVersion;
                                            if (parameters.size() > 0 && parameters[0].size() > 0 && parameters[0][0].starts_with("downloadEntry=")) {
                                                package["downloadEntry"] = parameters[0][0].substr(14); // skip "downloadEntry="
                                            } else {
                                                log("Overlay Updater:ERROR: downloadEntry not found for item \"%s\"", overlayName.c_str());
                                                break;
                                            }
                                            std::map<std::string, std::string> resultUpdate = ovlUpdateCheck(package);
                                            if (!resultUpdate.empty()) {
                                                NeedUpdate = true;
                                                items.insert(items.end(), resultUpdate);
                                            }
                                        }
                                    }
                                }
                            } else {
                                log("Overlay Updater:ERROR: Failed to download Updater.ini");
                            }
                        } else {
                            auto [result, overlayName, overlayVersion] = getOverlayInfo("sdmc:/switch/.overlays/ovlmenu.ovl");
                            if (result != ResultSuccess)
                                return false;
                            std::map<std::string, std::string> ovlmenu;
                            ovlmenu["name"] = overlayName;
                            ovlmenu["localVer"] = overlayVersion;
                            ovlmenu["link"] = "https://api.github.com/repos/efosamark/Uberhand-Overlay/releases?per_page=1";
                            ovlmenu["downloadEntry"] = "1";
                            std::map<std::string, std::string> resultUpdate = ovlUpdateCheck(ovlmenu);
                            if (!resultUpdate.empty()) {
                                NeedUpdate = true;
                                items.insert(items.end(), resultUpdate);
                            }
                        }

                        if (NeedUpdate) {
                            DownloadProcessing = false;
                            tsl::changeTo<Updater>(items);
                        }
                        updaterItem->setText("No updates");
                        DownloadProcessing = false;
                        return true;
                    }
                    return false;
                });
                list->addItem(updaterItem);
            }
        }

        if (menuMode == "packages") {
            // Create the directory if it doesn't exist
            createDirectory(packageDirectory);

            // Load options from INI file
            std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>> options = loadOptionsFromIni(packageConfigIniPath, true);

            // Load subdirectories
            std::vector<std::string> subdirectories = getSubdirectories(packageDirectory);

            FILE* packagesIniFile = fopen(packagesIniFilePath.c_str(), "r");
            if (!packagesIniFile) {
                fclose(fopen(packagesIniFilePath.c_str(), "w")); // The INI file doesn't exist, so create an empty one.
            } else {
                fclose(packagesIniFile); // The file exists, so close it.
            }
            std::multimap<int, std::string> order;
            std::map<std::string, std::map<std::string, std::string>> packagesIniData = getParsedDataFromIniFile(packagesIniFilePath);
            std::sort(subdirectories.begin(), subdirectories.end());
            for (const auto& taintedSubdirectory : subdirectories) {
                priority = 0;
                std::string subWithoutSpaces = taintedSubdirectory;
                (void)std::remove(subWithoutSpaces.begin(), subWithoutSpaces.end(), ' ');
                if (packagesIniData.find(subWithoutSpaces) == packagesIniData.end()) {
                    setIniFileValue(packagesIniFilePath, subWithoutSpaces, "priority", "0");
                } else {
                    if (packagesIniData.find(subWithoutSpaces) != packagesIniData.end() && packagesIniData[subWithoutSpaces].find("priority") != packagesIniData[subWithoutSpaces].end()) {
                        priority = -(stoi(packagesIniData[subWithoutSpaces]["priority"]));
                    } else
                        setIniFileValue(packagesIniFilePath, subWithoutSpaces, "priority", "0");
                }
                // log("priority, taintedSubdirectory: "+std::to_string(priority)+subWithoutSpaces);
                order.emplace(priority, taintedSubdirectory);
            }

            count = 0;
            for (const auto& package : order) {

                //bool usingStar = false;
                std::string subdirectory = package.second;
                std::string subdirectoryIcon = "";

                std::string subPath = packageDirectory + subdirectory + "/";
                std::string configFilePath = subPath + "config.ini";

                if (isFileOrDirectory(configFilePath)) {
                    PackageHeader packageHeader = getPackageHeaderFromIni(subPath + configFileName);
                    if (count == 0) {
                        // Add a section break with small text to indicate the "Packages" section
                        list->addItem(new CustomCategoryHeader("Packages"));
                    }

                    auto listItem = new tsl::elm::ListItem(subdirectoryIcon + subdirectory);
                    if (showPackageVersions)
                        listItem->setValue(packageHeader.version);

                    std::string subWithoutSpaces;
                    if (sorting) {
                        std::map<std::string, std::map<std::string, std::string>> packagesIniData = getParsedDataFromIniFile(packagesIniFilePath);
                        subWithoutSpaces = subdirectory;
                        (void)std::remove(subWithoutSpaces.begin(), subWithoutSpaces.end(), ' ');
                        priority = std::stoi(packagesIniData[subWithoutSpaces]["priority"]);
                        listItem->setValue("Priority: " + std::to_string(priority));
                    }
                    listItem->setClickListener([this, listItem, subdirectory, subWithoutSpaces, subPath = packageDirectory + subdirectory + "/"](uint64_t keys) {
                        int localPriority;
                        std::string subWithoutSpaces;
                        if (keys & KEY_A) {
                            if (isFileOrDirectory(subPath + "/init.ini")) {
                                if (!DownloadProcessing) {
                                    DownloadProcessing = true;
                                    listItem->setText("Processing...");
                                    return true;
                                }
                            } else {
                                tsl::changeTo<Package>(subPath);
                            }
                            return true;
                        } else if (keys & KEY_PLUS) {
                            std::map<std::string, std::map<std::string, std::string>> packagesIniData = getParsedDataFromIniFile(packagesIniFilePath);
                            subWithoutSpaces = subdirectory;
                            (void)std::remove(subWithoutSpaces.begin(), subWithoutSpaces.end(), ' ');
                            localPriority = std::stoi(packagesIniData[subWithoutSpaces]["priority"]) + 1;
                            setIniFileValue(packagesIniFilePath, subWithoutSpaces, "priority", std::to_string(localPriority));
                            tsl::replaceWith<MainMenu>(Spackages);
                            return true;
                        } else if (keys & KEY_MINUS) {
                            std::map<std::string, std::map<std::string, std::string>> packagesIniData = getParsedDataFromIniFile(packagesIniFilePath);
                            subWithoutSpaces = subdirectory;
                            (void)std::remove(subWithoutSpaces.begin(), subWithoutSpaces.end(), ' ');
                            localPriority = std::stoi(packagesIniData[subWithoutSpaces]["priority"]) - 1;
                            setIniFileValue(packagesIniFilePath, subWithoutSpaces, "priority", std::to_string(localPriority));
                            tsl::replaceWith<MainMenu>(Spackages);
                            return true;
                        }
                        if (DownloadProcessing) {
                            std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>> options = loadOptionsFromIni(subPath + "/init.ini");
                            for (const auto& option : options) {
                                if (interpretAndExecuteCommand(getModifyCommands(option.second, subPath + option.first)) == -1) {
                                    log("Init failed!");
                                    DownloadProcessing = false;
                                    listItem->setValue("FAIL", tsl::PredefinedColors::Red);
                                } else {
                                    deleteFileOrDirectory(subPath + "/init.ini");
                                    DownloadProcessing = false;
                                    tsl::changeTo<Package>(subPath);
                                }
                            }
                        }
                        return false;
                    });

                    list->addItem(listItem);
                    count++;
                }
            }

            //package updater section
            if (packageUpdater) {
                list->addItem(new CustomCategoryHeader("Updater", true));
                auto updaterItem = new tsl::elm::ListItem("Check for Package Updates");
                updaterItem->setClickListener([this, subdirectories, updaterItem](uint64_t keys) {
                    if (keys & KEY_A) {
                        if (!DownloadProcessing) {
                            DownloadProcessing = true;
                            updaterItem->setText("Processing...");
                            return true;
                        }
                    }
                    if (DownloadProcessing) {
                        bool NeedUpdate = false;
                        std::vector<std::map<std::string, std::string>> items;
                        for (const auto& taintedSubdirectory : subdirectories) {
                            std::map<std::string, std::string> packageInfo = packageUpdateCheck(taintedSubdirectory + "/config.ini");
                            if (packageInfo["localVer"] != packageInfo["repoVer"]) {
                                NeedUpdate = true;
                                packageInfo["filename"] = packageDirectory + taintedSubdirectory;
                                items.insert(items.end(), packageInfo);
                            }
                        }
                        if (NeedUpdate) {
                            DownloadProcessing = false;
                            tsl::changeTo<Updater>(items);
                        }
                        DownloadProcessing = false;
                        updaterItem->setText("No updates");
                        return true;
                    }
                    return false;
                });
                list->addItem(updaterItem);
            }
            count = 0;
            //std::string optionName;
            // Populate the menu with options
            for (const auto& option : options) {
                optionName = option.first;

                // Check if it's a subdirectory
                fullPath = packageDirectory + optionName;
                if (count == 0) {
                    // Add a section break with small text to indicate the "Packages" section
                    list->addItem(new CustomCategoryHeader("Commands"));
                }

                if (option.second.front()[0] == "separator") {
                    auto item = new CustomCategoryHeader(optionName, true);
                    list->addItem(item);
                    continue;
                }

                //std::string header;
                //if ((optionName == "Shutdown")) {
                //    header = "\uE0F3  ";
                //}
                //else if ((optionName == "Safe Reboot") || (optionName == "L4T Reboot")) {
                //    header = "\u2194  ";
                //}
                //auto listItem = new tsl::elm::ListItem(header+optionName);
                auto listItem = new tsl::elm::ListItem(optionName);

                std::vector<std::vector<std::string>> modifiedCommands = getModifyCommands(option.second, fullPath);
                listItem->setClickListener([this, command = modifiedCommands, subPath = optionName, listItem](uint64_t keys) {
                    if (keys & KEY_A) {
                        // Check if it's a subdirectory
                        struct stat entryStat;
                        std::string newPath = packageDirectory + subPath;
                        if (stat(fullPath.c_str(), &entryStat) == 0 && S_ISDIR(entryStat.st_mode)) {
                            tsl::changeTo<SubMenu>(newPath);
                        } else {
                            // Interpret and execute the command
                            int result = interpretAndExecuteCommand(command);
                            if (result == 0) {
                                listItem->setValue("DONE", tsl::PredefinedColors::Green);
                            } else if (result == 1) {
                                tsl::goBack();
                            } else {
                                listItem->setValue("FAIL", tsl::PredefinedColors::Red);
                            }
                        }

                        return true;
                    } else if (keys && (listItem->getValue() == "DONE" || listItem->getValue() == "FAIL")) {
                        listItem->setValue("");
                    }
                    return false;
                });

                list->addItem(listItem);
                count++;
            }
        }

        rootFrame->setContent(list);

        return rootFrame;
    }

    bool adjuct_top, adjuct_bot = false;

    virtual bool handleInput(uint64_t keysDown, uint64_t keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override
    {
        if (this->adjuct_top) { // Adjust cursor to the top item after jump bot-top
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            this->adjuct_top = false;
            return true;
        }
        if (this->adjuct_bot) { // Adjust cursor to the top item after jump top-bot
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            this->adjuct_bot = false;
            return true;
        }
        // Jump bot-top: scroll to the top after hitting the last list item
        if ((keysDown & KEY_DDOWN) || (keysDown & KEY_DOWN)) {
            auto prevItem = this->getFocusedElement();
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            if (prevItem == this->getFocusedElement()) {
                scrollListItems(this, ShiftFocusMode::UpMax);
                this->adjuct_top = true; // Go one item
            } else { // Adjust to account for tesla key processing
                this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            }
            return false;
        }
        // Jump bot-top: scroll to the top after hitting the last list item
        if ((keysDown & KEY_DUP) || (keysDown & KEY_UP)) {
            auto prevItem = this->getFocusedElement();
            this->requestFocus(this->getTopElement(), tsl::FocusDirection::Up);
            if (prevItem == this->getFocusedElement()) {
                scrollListItems(this, ShiftFocusMode::DownMax);
                this->adjuct_bot = true;
            } else { // Adjust to account for tesla key processing
                this->requestFocus(this->getTopElement(), tsl::FocusDirection::Down);
            }
            return false;
        }
        if (keysDown & KEY_L) { // Scroll to up for 5 items
            scrollListItems(this, ShiftFocusMode::UpNum);
            return true;
        }
        if (keysDown & KEY_R) { // Scroll to down for 5 items
            scrollListItems(this, ShiftFocusMode::DownNum);
            return true;
        }
        if (keysDown & KEY_ZL) { // Scroll to up for 5 items
            scrollListItems(this, ShiftFocusMode::UpMax);
            return true;
        }
        if (keysDown & KEY_ZR) { // Scroll to down for 5 items
            scrollListItems(this, ShiftFocusMode::DownMax);
            return true;
        }
        if ((keysDown & KEY_DRIGHT) && !(keysHeld & ~KEY_DRIGHT)) {
            if (menuMode != "packages") {
                setIniFileValue(settingsConfigIniPath, "uberhand", "last_menu", "packages");
                tsl::replaceWith<MainMenu>(Packages);
                return true;
            }
        }
        if ((keysDown & KEY_DLEFT) && !(keysHeld & ~KEY_DLEFT)) {
            if (menuMode != "overlays") {
                setIniFileValue(settingsConfigIniPath, "uberhand", "last_menu", "overlays");
                tsl::replaceWith<MainMenu>(Overlays);
                return true;
            }
        }
        if (keysDown & KEY_B) {
            tsl::Overlay::get()->close();
            return true;
        }
        return false;
    }
};

class Overlay : public tsl::Overlay {
public:
    virtual void initServices() override
    {
        fsdevMountSdmc();
        splInitialize();
        spsmInitialize();
        ASSERT_FATAL(socketInitializeDefault());
        ASSERT_FATAL(nifmInitialize(NifmServiceType_User));
        ASSERT_FATAL(timeInitialize());
        ASSERT_FATAL(smInitialize());
    }

    virtual void exitServices() override
    {
        socketExit();
        nifmExit();
        timeExit();
        smExit();
        spsmExit();
        splExit();
        fsdevUnmountAll();
    }

    virtual void onShow() override
    {
        //if (rootFrame != nullptr) {
        //    tsl::Overlay::get()->getCurrentGui()->removeFocus();
        //    rootFrame->invalidate();
        //    tsl::Overlay::get()->getCurrentGui()->requestFocus(rootFrame, tsl::FocusDirection::None);
        //}
    } // Called before overlay wants to change from invisible to visible state
    virtual void onHide() override { } // Called before overlay wants to change from visible to invisible state

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override
    {
        return initially<MainMenu>(); // Initial Gui to load. It's possible to pass arguments to its constructor like this
    }
};

int main(int argc, char* argv[])
{
    return tsl::loop<Overlay, tsl::impl::LaunchFlags::None>(argc, argv);
}
