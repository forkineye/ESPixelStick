#ifdef SUPPORT_OLED
#include "service/DisplayOLED.h"
#include <Preferences.h>

#define TOAST_DURATION_MS          5000
#define NETWORK_UPDATE_INTERVAL_MS 10000
#define BUTTON_DEBOUNCE_DELAY_MS   1000
#define VERSION_STRING             "4.x-dev"

static bool isRebooting = false;
static SemaphoreHandle_t displayMutex = nullptr;
static TaskHandle_t oledTaskHandle = nullptr;

static U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

static void OLEDTask(void *) {
    while (true) {
        if (displayMutex && xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            OLED.Poll();
            xSemaphoreGive(displayMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(OLED_TASK_DELAY_MS));
    }
}

c_OLED::c_OLED()
    : currentPage(DisplayPage::NETWORK_INFO), lastPageSwitchTime(0), lastNetworkUpdate(0),
      isUploading(false), uploadProgress(0), lastUploadUpdate(0), dispRSSI(0),
      isToastActive(false), toastStartTime(0), flipState(false), preferences() {}

void c_OLED::Begin() {
    displayMutex = xSemaphoreCreateMutex();
    if (!displayMutex) return;

    preferences.begin("oled", false);
    flipState = preferences.getBool("flipState", false);
    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);
    if (!u8g2.begin()) return;

    xTaskCreatePinnedToCore(OLEDTask, "OLEDTask", 2048, nullptr, 1, &oledTaskHandle, 0);

#ifdef BUTTON_GPIO1
    pinMode(BUTTON_GPIO1, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_GPIO1), buttonISR, FALLING);
#endif

    InitNow();
}

static void DrawCenteredWrappedText(U8G2 &u8g2, const String &text, uint8_t maxWidth, uint8_t xOrigin, uint8_t yOrigin) {
    constexpr uint8_t DISPLAY_WIDTH = 128;
    constexpr uint8_t DISPLAY_HEIGHT = 32;

    auto setFont = [&](bool large) {
        u8g2.setFont(large ? u8g2_font_10x20_tf : u8g2_font_6x10_tf);
    };

    std::vector<String> lines;
    uint8_t lineHeight = 0;
    bool fontFit = false;

    for (int attempt = 0; attempt < 2; attempt++) {
        setFont(attempt == 0);
        lines.clear();
        String currentLine, currentWord;

        for (uint16_t i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if (c == ' ' || c == '\n') {
                if (u8g2.getStrWidth((currentLine + currentWord).c_str()) > maxWidth) {
                    lines.push_back(currentLine);
                    currentLine = currentWord + " ";
                } else {
                    currentLine += currentWord + " ";
                }
                currentWord = "";
                if (c == '\n') { lines.push_back(currentLine); currentLine = ""; }
            } else {
                currentWord += c;
            }
        }

        if (!currentWord.isEmpty()) {
            if (u8g2.getStrWidth((currentLine + currentWord).c_str()) > maxWidth) {
                lines.push_back(currentLine);
                currentLine = currentWord;
            } else {
                currentLine += currentWord;
            }
        }
        if (!currentLine.isEmpty()) lines.push_back(currentLine);

        lineHeight = u8g2.getAscent() - u8g2.getDescent() + 2;
        if (lineHeight * lines.size() <= DISPLAY_HEIGHT) {
            fontFit = true;
            break;
        }
    }

    if (!fontFit) {
        setFont(false);
        lineHeight = u8g2.getAscent() - u8g2.getDescent() + 2;
    }

    uint8_t totalHeight = lineHeight * lines.size();
    uint8_t startY = yOrigin + ((DISPLAY_HEIGHT - totalHeight) / 2);

    for (size_t i = 0; i < lines.size(); i++) {
        uint8_t strWidth = u8g2.getStrWidth(lines[i].c_str());
        uint8_t x = xOrigin + ((DISPLAY_WIDTH - strWidth) / 2);
        uint8_t y = startY + (i * lineHeight);
        u8g2.drawStr(x, y, lines[i].c_str());
    }
}


void c_OLED::InitNow()
{
    if (!displayMutex || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_t0_12b_tr);
    u8g2.drawStr(28, 28, "ESPixelStick");
    u8g2.setFont(u8g2_font_profont22_tr);
    u8g2.drawStr(10, 17, "INITIALZE");
    u8g2.setDrawColor(2);
    u8g2.drawFrame(0, 0, 128, 31);
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(112, 27, "4.x");
    u8g2.drawStr(5, 27, "DEV");
    u8g2.sendBuffer();

    xSemaphoreGive(displayMutex);
}

void c_OLED::End()
{
    if (oledTaskHandle) { vTaskDelete(oledTaskHandle); oledTaskHandle = nullptr; }
    if (displayMutex) { vSemaphoreDelete(displayMutex); displayMutex = nullptr; }
    preferences.end();
}

void c_OLED::UpdateNetworkInfo(bool forceUpdate)
{
    if (!displayMutex || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    String currIP = NetworkMgr.GetlocalIP().toString();
    String currHost; NetworkMgr.GetHostname(currHost);
    int signalStrength = getSignalStrength(WiFi.RSSI());

    if (forceUpdate || currIP != dispIP || currHost != dispHostName || signalStrength != dispRSSI)
    {
        u8g2.clearBuffer();
        for (int i = 0; i < signalStrength; i++) {
            u8g2.drawBox(105 + i * 6, 16 - (i + 1) * 4, 4, (i + 1) * 4);
        }
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.drawStr(0, 18, ("IP: " + currIP).c_str());
        u8g2.drawStr(0, 30, ("HOST: " + currHost).c_str());
        dispIP = currIP; dispHostName = currHost; dispRSSI = signalStrength;
        u8g2.sendBuffer();
    }

    xSemaphoreGive(displayMutex);
}

void c_OLED::UpdateRunningStatus()
{
    if (!displayMutex || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    static JsonDocument doc;
    doc.clear();
    JsonObject jsonStatus = doc.to<JsonObject>();
    FPPDiscovery.GetStatus(jsonStatus);

    if (jsonStatus.isNull() || !jsonStatus["current_sequence"].is<String>()) {
        xSemaphoreGive(displayMutex);
        return;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, jsonStatus["current_sequence"].as<String>().c_str());
    u8g2.drawStr(0, 20, (jsonStatus["time_elapsed"].as<String>() + " / " + jsonStatus["time_remaining"].as<String>()).c_str());
    u8g2.sendBuffer();

    xSemaphoreGive(displayMutex);
}

void c_OLED::Update(bool forceUpdate)
{
    if (isRebooting) return;
    if (!displayMutex || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    unsigned long now = millis();
    if (currentPage == DisplayPage::NETWORK_INFO && (forceUpdate || now - lastNetworkUpdate >= NETWORK_UPDATE_INTERVAL_MS)) {
        xSemaphoreGive(displayMutex);
        UpdateNetworkInfo(forceUpdate);
        lastNetworkUpdate = now;
        return;
    }
    if (currentPage == DisplayPage::RUNNING_STATUS) {
        xSemaphoreGive(displayMutex);
        UpdateRunningStatus();
        return;
    }
    xSemaphoreGive(displayMutex);
}

void c_OLED::UpdateUploadStatus(const String &filename, int progress)
{
    if (isRebooting || !displayMutex || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    uploadFilename = filename;
    uploadProgress = progress;
    isUploading = true;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 12, uploadFilename.c_str());
    u8g2.drawStr(0, 28, ("Expand: " + String(progress) + "%").c_str());
    u8g2.drawFrame(0, 30, 128, 2);
    u8g2.drawBox(0, 30, map(progress, 0, 100, 0, 128), 2);
    u8g2.sendBuffer();

    xSemaphoreGive(displayMutex);
}

void c_OLED::ShowToast(const String &message)
{
    if (isRebooting || !displayMutex || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    toastMessage = message;
    isToastActive = true;
    toastStartTime = millis();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    DrawCenteredWrappedText(u8g2, toastMessage, 120, 0, 0);
    u8g2.sendBuffer();

    xSemaphoreGive(displayMutex);
}

void c_OLED::Poll()
{
    static unsigned long lastDebounceTime = 0;
    uint32_t notificationValue = 0;

    if (xTaskNotifyWait(0, ULONG_MAX, &notificationValue, 0) == pdTRUE) {
        if ((millis() - lastDebounceTime) > BUTTON_DEBOUNCE_DELAY_MS) {
            Flip();
            Update(true);
            lastDebounceTime = millis();
        }
    }

#ifdef BUTTON_GPIO1
    if (digitalRead(BUTTON_GPIO1) == LOW && (millis() - lastDebounceTime) > BUTTON_DEBOUNCE_DELAY_MS) {
        Flip();
        Update(true);
        lastDebounceTime = millis();
    }
#endif

    if (!isUploading && millis() - lastPageSwitchTime >= pageSwitchInterval) {
        currentPage = (FPPDiscovery.PlayingAfile() && currentPage == DisplayPage::NETWORK_INFO)
                        ? DisplayPage::RUNNING_STATUS : DisplayPage::NETWORK_INFO;
        lastPageSwitchTime = millis();
        Update(true);
    }

    if (isToastActive && millis() - toastStartTime >= TOAST_DURATION_MS) {
        isToastActive = false;
        Update(true);
    }
}

void c_OLED::Flip()
{
    if (isRebooting || !displayMutex || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    flipState = !flipState;
    u8g2.setDisplayRotation(flipState ? U8G2_R2 : U8G2_R0);
    preferences.putBool("flipState", flipState);
    u8g2.updateDisplay();
    xSemaphoreGive(displayMutex);
}

void c_OLED::ShowRebootScreen()
{
    if (!displayMutex || xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    isRebooting = true;
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(22, 12, "ESPixelStick");
    u8g2.setFont(u8g2_font_t0_22b_tr);
    u8g2.drawStr(15, 29, "REBOOTING");
    u8g2.sendBuffer();
    xSemaphoreGive(displayMutex);
    ESP.restart();
}

int c_OLED::getSignalStrength(int rssi)
{
    if (rssi > -50) return 4;
    if (rssi > -60) return 3;
    if (rssi > -70) return 2;
    if (rssi > -80) return 1;
    return 0;
}

void IRAM_ATTR c_OLED::buttonISR()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (oledTaskHandle) {
        xTaskNotifyFromISR(oledTaskHandle, 1, eSetBits, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
    }
}

c_OLED OLED;
#endif
