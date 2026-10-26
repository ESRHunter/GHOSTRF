#include "buttons.h"
#include "config.h"

struct ButtonState {
    uint8_t pin;
    bool isPressed;
    bool holdFired;
    unsigned long pressStartTime;
    unsigned long lastReleaseTime;
};

static ButtonState btnUp;
static ButtonState btnDown;
static ButtonState btnOk;

#include "driver/gpio.h"

void buttons_init() {
    gpio_num_t pins[] = { (gpio_num_t)PIN_BTN_UP, (gpio_num_t)PIN_BTN_ACTION, (gpio_num_t)PIN_BTN_DOWN };
    for (int i = 0; i < 3; i++) {
        gpio_reset_pin(pins[i]);
        gpio_set_direction(pins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(pins[i], GPIO_PULLUP_ONLY);
    }

    pinMode(PIN_BTN_UP, INPUT_PULLUP);
    pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
    pinMode(PIN_BTN_ACTION, INPUT_PULLUP);

    btnUp = { PIN_BTN_UP, false, false, 0, 0 };
    btnDown = { PIN_BTN_DOWN, false, false, 0, 0 };
    btnOk = { PIN_BTN_ACTION, false, false, 0, 0 };
}

static ButtonEvent checkSimpleButton(ButtonState &btn, ButtonEvent clickEvt) {
    bool rawDown = (digitalRead(btn.pin) == LOW);
    unsigned long now = millis();

    if (rawDown) {
        if (!btn.isPressed && (now - btn.lastReleaseTime >= DEBOUNCE_TIME_MS)) {
            btn.isPressed = true;
            btn.pressStartTime = now;
            return clickEvt;
        }
    } else {
        if (btn.isPressed) {
            btn.isPressed = false;
            btn.lastReleaseTime = now;
        }
    }
    return BTN_EVT_NONE;
}

static ButtonEvent checkOkButton(ButtonState &btn) {
    bool rawDown = (digitalRead(btn.pin) == LOW);
    unsigned long now = millis();

    if (rawDown) {
        if (!btn.isPressed) {
            if (now - btn.lastReleaseTime >= DEBOUNCE_TIME_MS) {
                btn.isPressed = true;
                btn.holdFired = false;
                btn.pressStartTime = now;
            }
        } else {
            // Кнопка удерживается: проверяем порог долгого нажатия (> 750 мс)
            if (!btn.holdFired && (now - btn.pressStartTime >= LONG_PRESS_TIME_MS)) {
                btn.holdFired = true;
                return BTN_EVT_OK_HOLD;
            }
        }
    } else {
        if (btn.isPressed) {
            btn.isPressed = false;
            btn.lastReleaseTime = now;
            // Если отпустили раньше срабатывания удержания и дольше антидребезга — это клик
            if (!btn.holdFired && (now - btn.pressStartTime >= DEBOUNCE_TIME_MS)) {
                return BTN_EVT_OK_CLICK;
            }
        }
    }
    return BTN_EVT_NONE;
}

static ButtonEvent injectedEvt = BTN_EVT_NONE;

void buttons_inject_event(ButtonEvent evt) {
    injectedEvt = evt;
}

ButtonEvent buttons_poll() {
    if (injectedEvt != BTN_EVT_NONE) {
        ButtonEvent e = injectedEvt;
        injectedEvt = BTN_EVT_NONE;
        return e;
    }

    // Временно блокируем спам событий, пока пин зажат
    ButtonEvent evt = checkOkButton(btnOk);
    if (evt != BTN_EVT_NONE) return evt;

    evt = checkSimpleButton(btnUp, BTN_EVT_UP_CLICK);
    if (evt != BTN_EVT_NONE) return evt;

    evt = checkSimpleButton(btnDown, BTN_EVT_DOWN_CLICK);
    if (evt != BTN_EVT_NONE) return evt;

    return BTN_EVT_NONE;
}
