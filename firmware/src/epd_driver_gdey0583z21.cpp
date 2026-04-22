#include "epd_driver.h"
#include "config.h"

#if defined(EPD_PANEL_583_GDEY0583Z21)

// ── Software SPI (bit-bang) ──────────────────────────────────
// Same approach as other panels in this project.

static void spiWriteByte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        digitalWrite(PIN_EPD_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        digitalWrite(PIN_EPD_SCK, HIGH);
        digitalWrite(PIN_EPD_SCK, LOW);
    }
}

static void epdSendCommand(uint8_t cmd) {
    digitalWrite(PIN_EPD_DC, LOW);
    digitalWrite(PIN_EPD_CS, LOW);
    spiWriteByte(cmd);
    digitalWrite(PIN_EPD_CS, HIGH);
}

static void epdSendData(uint8_t data) {
    digitalWrite(PIN_EPD_DC, HIGH);
    digitalWrite(PIN_EPD_CS, LOW);
    spiWriteByte(data);
    digitalWrite(PIN_EPD_CS, HIGH);
}

// BUSY=HIGH means panel is ready / operation complete (opposite polarity from SSD1683-BW).
// The 100ms initial delay matches Waveshare's ReadBusyH: without it, BUSY may not have
// gone LOW yet when we first sample it, causing a false "ready" and sending data to a
// panel that hasn't finished processing the previous command.
static void epdWaitBusy(unsigned long maxMs = 35000) {
    delay(100);
    unsigned long t0 = millis();
    while (digitalRead(PIN_EPD_BUSY) == LOW) {
        delay(5);
        if (millis() - t0 > maxMs) {
            Serial.println("[EPD-583G] busy timeout");
            return;
        }
    }
}

static void epdReset() {
    digitalWrite(PIN_EPD_RST, HIGH); delay(200);
    digitalWrite(PIN_EPD_RST, LOW);  delay(2);
    digitalWrite(PIN_EPD_RST, HIGH); delay(200);
}

// ── GPIO init ────────────────────────────────────────────────

void gpioInit() {
    pinMode(PIN_EPD_BUSY, INPUT);
    pinMode(PIN_EPD_RST,  OUTPUT);
    pinMode(PIN_EPD_DC,   OUTPUT);
    pinMode(PIN_EPD_CS,   OUTPUT);
    pinMode(PIN_EPD_SCK,  OUTPUT);
    pinMode(PIN_EPD_MOSI, OUTPUT);
    pinMode(PIN_CFG_BTN,  INPUT_PULLUP);
    digitalWrite(PIN_EPD_RST, HIGH);
    digitalWrite(PIN_EPD_CS,  HIGH);
    digitalWrite(PIN_EPD_SCK, LOW);
}

// ── Init sequences (from Waveshare EPD_5in83g reference) ─────

void epdInit() {
    epdReset();

    epdSendCommand(0x4D);
    epdSendData(0x78);

    epdSendCommand(0x00); // PSR: 3/4-color mode
    epdSendData(0x2F);
    epdSendData(0x29);

    epdSendCommand(0xE3);
    epdSendData(0x88);

    epdSendCommand(0x50); // CDI: VCOM and data interval
    epdSendData(0x37);

    epdSendCommand(0x61); // resolution
    epdSendData(W >> 8);
    epdSendData(W & 0xFF);
    epdSendData(H >> 8);
    epdSendData(H & 0xFF);

    epdSendCommand(0x65);
    epdSendData(0x00);
    epdSendData(0x10);
    epdSendData(0x00);
    epdSendData(0x00);

    epdSendCommand(0xE9);
    epdSendData(0x01);

    epdSendCommand(0x30); // PLL / frame rate
    epdSendData(0x08);

    epdSendCommand(0x04); // power on
    epdWaitBusy();
}

void epdInitFast() {
    epdReset();

    epdSendCommand(0x4D);
    epdSendData(0x78);

    epdSendCommand(0x00);
    epdSendData(0x2F);
    epdSendData(0x29);

    epdSendCommand(0xE3);
    epdSendData(0x88);

    epdSendCommand(0x50);
    epdSendData(0x37);

    epdSendCommand(0x61);
    epdSendData(W >> 8);
    epdSendData(W & 0xFF);
    epdSendData(H >> 8);
    epdSendData(H & 0xFF);

    epdSendCommand(0x65);
    epdSendData(0x00);
    epdSendData(0x10);
    epdSendData(0x00);
    epdSendData(0x00);

    epdSendCommand(0xE9);
    epdSendData(0x01);

    epdSendCommand(0x04); // power on (no 0x30 for fast LUT)
    epdWaitBusy();

    epdSendCommand(0xE0);
    epdSendData(0x02);

    epdSendCommand(0xE6);
    epdSendData(0x68);

    epdSendCommand(0xA5);
    epdWaitBusy();
}

// ── Display functions ────────────────────────────────────────

static void epdTurnOnDisplay() {
    epdSendCommand(0x12); // display refresh
    epdSendData(0x00);
    epdWaitBusy();
}

// Send 2bpp data (COLOR_BUF_LEN bytes: 4 pixels/byte, 00=black 01=white 10=yellow 11=red)
static void epdSend2bpp(const uint8_t *buf) {
    epdSendCommand(0x10);
    for (int i = 0; i < COLOR_BUF_LEN; i++) {
        epdSendData(buf[i]);
    }
}

// Expand 1bpp image to 2bpp on-the-fly: 0=black(00), 1=white(01).
// Avoids needing a separate 2bpp conversion buffer.
static void epdSend1bppAs2bpp(const uint8_t *image) {
    const int rowBytes = W / 8;
    epdSendCommand(0x10);
    for (int j = 0; j < H; j++) {
        for (int i = 0; i < rowBytes; i++) {
            uint8_t src = image[j * rowBytes + i];
            // Each source byte covers 8 pixels → 2 output bytes (4 pixels each)
            uint8_t b0 = 0, b1 = 0;
            for (int p = 0; p < 4; p++) {
                if (src & (0x80 >> p))      b0 |= 0x01 << (6 - p * 2); // white=01
                if (src & (0x08 >> p))      b1 |= 0x01 << (6 - p * 2);
            }
            epdSendData(b0);
            epdSendData(b1);
        }
    }
}

void epdDisplay(const uint8_t *image) {
    epdInit();
    epdSend1bppAs2bpp(image);
    epdTurnOnDisplay();
}

void epdDisplay2bpp(const uint8_t *image2bpp) {
    epdInit();
    epdSend2bpp(image2bpp);
    epdTurnOnDisplay();
}

void epdDisplayFast(const uint8_t *image) {
    epdInitFast();
    epdSend1bppAs2bpp(image);
    epdTurnOnDisplay();
}

void epdPartialDisplay(uint8_t *data, int xStart, int yStart, int xEnd, int yEnd) {
    (void)data; (void)xStart; (void)yStart; (void)xEnd; (void)yEnd;
    epdDisplay(imgBuf);
}

// ── Sleep ────────────────────────────────────────────────────

void epdSleep() {
    epdSendCommand(0x02); // power off
    epdSendData(0x00);
    epdWaitBusy(5000);
    epdSendCommand(0x07); // deep sleep
    epdSendData(0xA5);
    delay(200);
}

#endif // EPD_PANEL_583_GDEY0583Z21