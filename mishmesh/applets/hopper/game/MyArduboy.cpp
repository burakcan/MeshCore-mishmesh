#include "MyArduboy.h"
#include <mishmesh/sound/SoundEngine.h>

PROGMEM static const uint32_t imgFont[] = {
    0x00000000, 0x00017000, 0x000C00C0, 0x0A7CA7CA, 0x0855F542, 0x19484253, 0x1251F55E, 0x00003000,
    0x00452700, 0x001C9440, 0x0519F314, 0x0411F104, 0x00000400, 0x04104104, 0x00000400, 0x01084210,
    0x0F45145E, 0x0001F040, 0x13555559, 0x0D5D5551, 0x087C928C, 0x0D555557, 0x0D55555E, 0x010C5251,
    0x0F55555E, 0x0F555556, 0x0000A000, 0x0000A400, 0x0028C200, 0x0028A280, 0x00086280, 0x000D5040,
    0x0018E300, 0x1F24929C, 0x0D5D555F, 0x1145149C, 0x0725145F, 0x1155555F, 0x0114515F, 0x1D55545E,
    0x1F10411F, 0x0045F440, 0x07210410, 0x1D18411F, 0x1041041F, 0x1F04F05E, 0x1F04109C, 0x0F45545E,
    0x0314925F, 0x1F45D45E, 0x1B34925F, 0x0D555556, 0x0105F041, 0x0721041F, 0x0108421F, 0x0F41E41F,
    0x1D184317, 0x0109C107, 0x114D5651, 0x0045F000, 0x0001F000, 0x0001F440, 0x000C1080, 0x10410410,
};

#ifdef USE_ARDUBOY2_LIB

#define myAudio     audio

// [mishmesh] Constructor: pTunes/ArduboyPlaytune removed (audio out of scope).
// Original body:
//   pTunes = new ArduboyPlaytune(myAudio.enabled);
//   pTunes->initChannel(PIN_SPEAKER_1);
MyArduboy::MyArduboy(void)
{
}

void MyArduboy::beginNoLogo(void)
{
    boot();
    blank();
    flashlight();
    systemButtons();
    myAudio.begin();
}

#else

#define textSize    textsize
#define textWrap    wrap
#define pTunes      (&tunes)

void MyArduboyAudio::begin()
{
    if (EEPROM.read(EEPROM_AUDIO_ON_OFF))
        on();
    else
        off();
}

void MyArduboyAudio::on()
{
    power_timer3_enable();
    audio_enabled = true;
}

void MyArduboyAudio::off()
{
    audio_enabled = false;
    power_timer3_disable();
}

void MyArduboyAudio::toggle()
{
    if (audio_enabled)
        off();
    else
        on();
}

void MyArduboy::beginNoLogo(void)
{
    boot();
    if (pressed(UP_BUTTON)) {
        sendLCDCommand(OLED_ALL_PIXELS_ON);
        setRGBled(255, 255, 255);
        power_timer0_disable();
        while (true) {
            idle(); // infinite loop
        }
    }
    pTunes->initChannel(PIN_SPEAKER_1);
    pinMode(PIN_SPEAKER_2, OUTPUT); // trick
    myAudio.begin();
}

#endif

/*----------------------------------------------------------------------------*/

bool MyArduboy::nextFrame(void)
{
    bool ret = ARDUBOY_LIB_CLASS::nextFrame();
    if (ret) {
        lastButtonState = currentButtonState;
        currentButtonState = buttonsState();
    }
    return ret;
}

// [mishmesh] Button-state snapshot used by hopperLoopStep() in place of nextFrame().
// The runtime gates frame timing externally; this captures held/released transitions
// without invoking Arduboy2Base::nextFrame()'s millis-based timer.
void MyArduboy::snapshotButtons(void)
{
    lastButtonState = currentButtonState;
    currentButtonState = buttonsState();
}

bool MyArduboy::buttonDown(uint8_t buttons)
{
    return currentButtonState & ~lastButtonState & buttons;
}

bool MyArduboy::buttonPressed(uint8_t buttons)
{
    return currentButtonState & buttons;
}


bool MyArduboy::buttonUp(uint8_t buttons)
{
    return ~currentButtonState & lastButtonState & buttons;
}

/*----------------------------------------------------------------------------*/

void MyArduboy::setTextColor(uint8_t color)
{
    setTextColor(color, (color == BLACK) ? WHITE : BLACK);
}

void MyArduboy::setTextColor(uint8_t color, uint8_t bg)
{
    textcolor = color;
    textbg = bg;
}

size_t MyArduboy::printEx(int16_t x, int16_t y, const char *p)
{
    setCursor(x, y);
    return print(p);
}

size_t MyArduboy::printEx(int16_t x, int16_t y, const __FlashStringHelper *p)
{
    setCursor(x, y);
    return print(p);
}

size_t MyArduboy::write(uint8_t c)
{
    if (c == '\n') {
        cursor_y += textSize * 6;
        cursor_x = 0;
    } else if (c >= ' ' && c <= '_') {
        myDrawChar(cursor_x, cursor_y, c, textcolor, textbg, textSize);
        cursor_x += textSize * 6;
        if (textWrap && (cursor_x > (WIDTH - textSize * 6))) write('\n');
    }
    return 1; // temporary
}

void MyArduboy::myDrawChar(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size)
{
    bool draw_bg = bg != color;

    if (x >= WIDTH || y >= HEIGHT || x + 5 * size < 0 || y + 6 * size < 0) return;
    uint32_t ptn = pgm_read_dword(imgFont + (c - ' '));
    if (size == 1) {
        for (int8_t i = 0; i < 6; i++) {
            for (int8_t j = 0; j < 6; j++) {
                bool draw_fg = ptn & 0x1;
                if (draw_fg || draw_bg) {
                    drawPixel(x + i, y + j, (draw_fg) ? color : bg);
                }
                ptn >>= 1;
            }
        }
    } else {
        for (int8_t i = 0; i < 6; i++) {
            for (int8_t j = 0; j < 6; j++) {
                bool draw_fg = ptn & 0x1;
                if (draw_fg || draw_bg) {
                    fillRect(x + i * size, y + j * size, size, size, (draw_fg) ? color : bg);
                }
                ptn >>= 1;
            }
        }
    }
}

/*----------------------------------------------------------------------------*/

void MyArduboy::drawRect2(int16_t x, int16_t y, uint8_t w, int8_t h, uint8_t color)
{
    drawFastHLine2(x, y, w, color);
    drawFastHLine2(x, y + h - 1, w, color);
    drawFastVLine2(x, y + 1, h - 2, color);
    drawFastVLine2(x + w - 1, y + 1, h - 2, color);
}

void MyArduboy::drawFastVLine2(int16_t x, int16_t y, int8_t h, uint8_t color)
{
    /*  Check parameters  */
    if (y < 0) {
        if (h <= -y) return;
        h += y;
        y = 0;
    }
    if (h <= 0 || y >= HEIGHT || x < 0 || x >= WIDTH) return;
    if (y + h > HEIGHT) h = HEIGHT - y;

    /*  Draw a vertical line  */
    uint8_t yOdd = y & 7;
    uint8_t d = 0xFF << yOdd;
    y -= yOdd;
    h += yOdd;
    for (buffer_t *p = getBuffer() + x + (y / 8) * WIDTH; h > 0; h -= 8, p += WIDTH) {
        if (h < 8) d &= 0xFF >> (8 - h);
        if (color == BLACK) {
            *p &= ~d;
        } else {
            *p |= d;
        }
        d = 0xFF;
    }
}

void MyArduboy::drawFastHLine2(int16_t x, int16_t y, uint8_t w, uint8_t color)
{
    /*  Check parameters  */
    if (x < 0) {
        if (w <= -x) return;
        w += x;
        x = 0;
    }
    if (w <= 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    if (x + w > WIDTH) w = WIDTH - x;

    /*  Draw a horizontal line  */
    uint8_t yOdd = y & 7;
    uint8_t d = 1 << yOdd;
    buffer_t *p = getBuffer() + x + (y / 8) * WIDTH;
    if (color == BLACK) {
        fillBeltBlack(p, d, w);
    } else {
        fillBeltWhite(p, d, w);
    }
}

void MyArduboy::fillRect2(int16_t x, int16_t y, uint8_t w, int8_t h, uint8_t color)
{
    /*  Check parameters  */
    if (x < 0) {
        if (w <= -x) return;
        w += x;
        x = 0;
    }
    if (y < 0) {
        if (h <= -y) return;
        h += y;
        y = 0;
    }
    if (w <= 0 || x >= WIDTH || h <= 0 || y >= HEIGHT) return;
    if (x + w > WIDTH) w = WIDTH - x;
    if (y + h > HEIGHT) h = HEIGHT - y;

    /*  Draw a filled rectangle  */
    uint8_t yOdd = y & 7;
    uint8_t d = 0xFF << yOdd;
    y -= yOdd;
    h += yOdd;
    for (buffer_t *p = getBuffer() + x + (y / 8) * WIDTH; h > 0; h -= 8, p += WIDTH) {
        if (h < 8) d &= 0xFF >> (8 - h);
        if (color == BLACK) {
            fillBeltBlack(p, d, w);
        } else {
            fillBeltWhite(p, d, w);
        }
        d = 0xFF;
    }
}

void MyArduboy::fillBeltBlack(buffer_t *p, uint8_t d, uint8_t w)
{
    d = ~d;
    for (; w > 0; w--) {
        *p++ &= d;
    }
}

void MyArduboy::fillBeltWhite(buffer_t *p, uint8_t d, uint8_t w)
{
    for (; w > 0; w--) {
        *p++ |= d;
    }
}

/*----------------------------------------------------------------------------*/

bool MyArduboy::isAudioEnabled(void)
{
    return myAudio.enabled();
}

void MyArduboy::setAudioEnabled(bool on)
{
    if (on) {
        myAudio.on();
    } else {
        myAudio.off();
    }

}

void MyArduboy::saveAudioOnOff(void)
{
    myAudio.saveOnOff();
}

// [mishmesh] route Arduboy scores to the mishmesh sound engine (1:1).
void MyArduboy::playScore2(const byte *score, uint8_t priority)
{
    (void)priority;   // engine arbitration handles priority via the exclusive lock
    if (!isAudioEnabled()) return;   // honor the game's SOUND on/off toggle (audio.begin/saveOnOff)
    if (auto* e = ::mishmesh::sound::activeEngine())
        e->playScore((const uint8_t *)score, ::mishmesh::sound::SoundCategory::Game);
}
void MyArduboy::stopScore2(void)
{
    if (auto* e = ::mishmesh::sound::activeEngine()) e->stop();
}
