// =========================================================================
// GHOSTRF — AUTHENTIC SSD1306 (128x64) OLED EMULATION ENGINE
// Mirrors src/ui.cpp, src/types.h, and src/main.cpp 1:1
// =========================================================================

// --- 1. BACKGROUND PIXEL RAIN STREAM (FALLING DIGITAL PARTICLES IN THE VOID) ---
(function initPixelRain() {
  const pCanvas = document.getElementById('bgPixelCanvas');
  if (!pCanvas) return;
  const pCtx = pCanvas.getContext('2d');
  let pW = (pCanvas.width = window.innerWidth);
  let pH = (pCanvas.height = window.innerHeight);

  window.addEventListener('resize', () => {
    pW = pCanvas.width = window.innerWidth;
    pH = pCanvas.height = window.innerHeight;
  });

  const NUM_PIXELS = 120;
  const pixels = [];
  const shades = [
    'rgba(255, 255, 255, 0.75)',
    'rgba(255, 255, 255, 0.45)',
    'rgba(210, 210, 210, 0.60)',
    'rgba(180, 180, 180, 0.35)',
    'rgba(120, 120, 120, 0.50)',
    'rgba(80, 80, 80, 0.30)'
  ];
  const sizes = [2, 2, 3, 3, 4];

  for (let i = 0; i < NUM_PIXELS; i++) {
    pixels.push({
      x: Math.random() * pW,
      y: Math.random() * pH,
      size: sizes[Math.floor(Math.random() * sizes.length)],
      color: shades[Math.floor(Math.random() * shades.length)],
      speed: 0.4 + Math.random() * 1.8,
      driftFreq: 0.005 + Math.random() * 0.015,
      driftAmp: 0.3 + Math.random() * 0.8,
      phase: Math.random() * Math.PI * 2
    });
  }

  function renderPixels() {
    pCtx.clearRect(0, 0, pW, pH);
    for (let i = 0; i < pixels.length; i++) {
      const p = pixels[i];
      p.y += p.speed;
      p.x += Math.sin(p.y * p.driftFreq + p.phase) * p.driftAmp;

      if (p.y > pH) {
        p.y = -5;
        p.x = Math.random() * pW;
      }
      if (p.x < -10) p.x = pW + 5;
      if (p.x > pW + 10) p.x = -5;

      pCtx.fillStyle = p.color;
      pCtx.fillRect(Math.floor(p.x), Math.floor(p.y), p.size, p.size);
    }
    requestAnimationFrame(renderPixels);
  }
  requestAnimationFrame(renderPixels);
})();

// --- 2. WEB AUDIO API SYNTHESIZER (TACTILE HARDWARE CLICKS & CHIRPS) ---
let audioCtx = null;
function getAudioContext() {
  if (!audioCtx) {
    audioCtx = new (window.AudioContext || window.webkitAudioContext)();
  }
  if (audioCtx && audioCtx.state === 'suspended') {
    audioCtx.resume();
  }
  return audioCtx;
}

function playClickSound(freq = 900, dur = 0.015) {
  try {
    const ctx = getAudioContext();
    if (!ctx) return;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'square';
    osc.frequency.setValueAtTime(freq, ctx.currentTime);
    gain.gain.setValueAtTime(0.06, ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + dur);
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.start();
    osc.stop(ctx.currentTime + dur);
  } catch (e) {}
}

function playChirpSound() {
  try {
    const ctx = getAudioContext();
    if (!ctx) return;
    const now = ctx.currentTime;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'square';
    osc.frequency.setValueAtTime(1400, now);
    osc.frequency.setValueAtTime(1900, now + 0.03);
    osc.frequency.setValueAtTime(2400, now + 0.06);
    gain.gain.setValueAtTime(0.07, now);
    gain.gain.exponentialRampToValueAtTime(0.001, now + 0.12);
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.start(now);
    osc.stop(now + 0.12);
  } catch (e) {}
}

function playToneSound(freq = 1000, dur = 0.2) {
  try {
    const ctx = getAudioContext();
    if (!ctx) return;
    const now = ctx.currentTime;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(freq, now);
    gain.gain.setValueAtTime(0.08, now);
    gain.gain.exponentialRampToValueAtTime(0.001, now + dur);
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.start(now);
    osc.stop(now + dur);
  } catch (e) {}
}

// --- 3. CLASSIC ADAFRUIT GFX 5x7 FONT (1:1 HARDWARE ROM BITMAP) ---
const GLCD_FONT_B64 = "AAAAAAA+W09bPj5rT2s+HD58PhwYPH48GBxXfVccHF5/XhwAGDwYAP/nw+f/ABgkGAD/59vn/zBIOgYOJil5KSZAfwUFB0B/BSU/WjznPFp/PhwcCAgcHD5/FCJ/IhRfXwBfXwYJfwF/AGaJlWpgYGBgYJSi/6KUCAR+BAgQIH4gEAgIKhwICBwqCAgeEBAQEAweDB4MMDg+ODAGDj4OBgAAAAAAAABfAAAABwAHABR/FH8UJCp/KhIjEwhkYjZJViBQAAgHAwAAHCJBAABBIhwAKhx/HCoICD4ICACAcDAACAgICAgAAGBgACAQCAQCPlFJRT4AQn9AAHJJSUlGIUFJTTMYFBJ/ECdFRUU5PEpJSTFBIREJBzZJSUk2RklJKR4AABQAAABANAAAAAgUIkEUFBQUFABBIhQIAgFZCQY+QV1ZTnwSERJ8f0lJSTY+QUFBIn9BQUE+f0lJSUF/CQkJAT5BQVFzfwgICH8AQX9BACBAQT8BfwgUIkF/QEBAQH8CHAJ/fwQIEH8+QUFBPn8JCQkGPkFRIV5/CRkpRiZJSUkyAwF/AQM/QEBAPx8gQCAfP0A4QD9jFAgUYwMEeAQDYVlJTUMAf0FBQQIECBAgAEFBQX8EAgECBEBAQEBAAAMHCAAgVFR4QH8oREQ4OERERCg4REQofzhUVFQYAAh+CQIYpKSceH8IBAR4AER9QAAgQEA9AH8QKEQAAEF/QAB8BHgEeHwIBAR4OERERDj8GCQkGBgkJBj8fAgEBAhIVFRUJAQEP0QkPEBAIHwcIEAgHDxAMEA8RCgQKERMkJCQfERkVExEAAg2QQAAAHcAAABBNggAAgECBAI8JiMmPB6hoWESOkBAIHo4VFRVWSFVVXlBIlRUeEIhVVR4QCBUVXlADB5SchI5VVVVWTlUVFRZOVVUVFgAAEV8QQACRX1CAAFFfEB9EhESffAoJSjwfFRVRQAgVFR8VHwKCX9JMklJSTI6REREOjJKSEgwOkFBIXo6QkAgeACdoKB9PUJCQj09QEBAPTwk/yQkSH5JQ2YrL/wvK/8JKfYgwIh+CQMgVFR5QQAARH1BMEhISjI4QEAiegB6CgpyfQ0ZMX0mKSkvKCYpKSkmMEhNQCA4CAgICAgICAg4LxDIrLovECg0+gAAewAACBQqFCIiFCoUCFUAVQBVqlWqVar/Vf9V/wAAAP8AEBAQ/wAUFBT/ABAQ/wD/EBDwEPAUFBT8ABQU9wD/AAD/AP8UFPQE/BQUFxAfEBAfEB8UFBQfABAQEPAAAAAAHxAQEBAfEBAQEPAQAAAA/xAQEBAQEBAQEP8QAAAA/xQAAP8A/wAAHxAXAAD8BPQUFBcQFxQU9AT0AAD/APcUFBQUFBQU9wD3FBQUFxQQEB8QHxQUFPQUEBDwEPAAAB8QHwAAAB8UAAAA/BQAAPAQ8BAQ/xD/FBQU/xQQEBAfAAAAAPAQ///////w8PDw8P///wAAAAAA//8PDw8PDzhERDhE/EpKSjR+AgIGBgJ+An4CY1VJQWM4REQ8BEB+IB4gBgJ+AgKZpeelmRwqSSocTHIBckwwSk1NMDBIeEgwvGJaRj0+SUlJAH4BAQF+KioqKipERF9EREBRSkRAQERKUUAAAP8BA+CA/wAACAhrawg2EjYkNgYPCQ8GAAAYGAAAABAQADBA/wEBAB8BAR4AGR0XEgA8PDw8AAAAAAA=";
const fontBytes = Uint8Array.from(atob(GLCD_FONT_B64), c => c.charCodeAt(0));

// 32x32 Pixel Mascot Bitmap directly from PROGMEM in src/ui.cpp
const ghostrf_bitmap_32x32 = [
  0x00, 0x3f, 0xf0, 0x00, 0x00, 0xff, 0xfe, 0x00,
  0x01, 0xff, 0xff, 0x00, 0x03, 0xff, 0xff, 0x80,
  0x03, 0xff, 0xff, 0xc0, 0x03, 0xff, 0xff, 0xe0,
  0x07, 0xe3, 0xf1, 0xf0, 0x07, 0xe3, 0xf1, 0xf0,
  0x07, 0xe3, 0xf1, 0xf0, 0x0f, 0xe3, 0xf1, 0xf0,
  0x1f, 0xe3, 0xf1, 0xf0, 0x1f, 0xe3, 0xf1, 0xf0,
  0x1f, 0xff, 0xff, 0xf0, 0x1f, 0xfe, 0x7f, 0xf0,
  0x1f, 0xfc, 0x3f, 0xf0, 0x1f, 0xfc, 0x3f, 0xf0,
  0x1f, 0xfc, 0x3f, 0xf8, 0x1f, 0xfe, 0x7f, 0xf8,
  0x3f, 0xff, 0xff, 0xf8, 0x3f, 0xff, 0xff, 0xfc,
  0x7f, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc,
  0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc,
  0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xf0,
  0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xc0,
  0x3f, 0xff, 0xff, 0x80, 0x0f, 0xff, 0xff, 0x00,
  0x06, 0x1f, 0x8e, 0x00, 0x00, 0x0f, 0x00, 0x00
];

// --- 4. HARDWARE SSD1306 (128x64) 1-BIT EMULATOR CLASS ---
class SSD1306Device {
  constructor(w = 128, h = 64) {
    this.width = w;
    this.height = h;
    this.buffer = new Uint8Array(w * h);
    this.cursorX = 0;
    this.cursorY = 0;
    this.textSize = 1;
    this.textColor = 1;
    this.textBg = 0;
  }

  clearDisplay() {
    this.buffer.fill(0);
  }

  drawPixel(x, y, color = 1) {
    if (x < 0 || x >= this.width || y < 0 || y >= this.height) return;
    this.buffer[y * this.width + x] = color ? 1 : 0;
  }

  getPixel(x, y) {
    if (x < 0 || x >= this.width || y < 0 || y >= this.height) return 0;
    return this.buffer[y * this.width + x];
  }

  drawFastHLine(x, y, w, color = 1) {
    if (y < 0 || y >= this.height) return;
    for (let i = 0; i < w; i++) {
      this.drawPixel(x + i, y, color);
    }
  }

  drawFastVLine(x, y, h, color = 1) {
    if (x < 0 || x >= this.width) return;
    for (let i = 0; i < h; i++) {
      this.drawPixel(x, y + i, color);
    }
  }

  drawRect(x, y, w, h, color = 1) {
    this.drawFastHLine(x, y, w, color);
    this.drawFastHLine(x, y + h - 1, w, color);
    this.drawFastVLine(x, y, h, color);
    this.drawFastVLine(x + w - 1, y, h, color);
  }

  fillRect(x, y, w, h, color = 1) {
    for (let j = 0; j < h; j++) {
      this.drawFastHLine(x, y + j, w, color);
    }
  }

  drawBitmap(x, y, bitmap, w, h, color = 1) {
    const byteWidth = Math.floor((w + 7) / 8);
    for (let j = 0; j < h; j++) {
      for (let i = 0; i < w; i++) {
        const byteIndex = j * byteWidth + Math.floor(i / 8);
        const bit = 7 - (i % 8);
        if ((bitmap[byteIndex] >> bit) & 1) {
          this.drawPixel(x + i, y + j, color);
        }
      }
    }
  }

  setCursor(x, y) {
    this.cursorX = x;
    this.cursorY = y;
  }

  setTextSize(s) {
    this.textSize = Math.max(1, s);
  }

  setTextColor(c, bg = -1) {
    this.textColor = c ? 1 : 0;
    this.textBg = bg === -1 ? -1 : (bg ? 1 : 0);
  }

  drawChar(x, y, ch, color, bg, size) {
    const code = ch.charCodeAt(0);
    if (code < 0 || code > 255) return;
    const offset = code * 5;

    for (let i = 0; i < 5; i++) {
      const line = fontBytes[offset + i];
      for (let j = 0; j < 8; j++) {
        const pixelOn = (line >> j) & 1;
        if (pixelOn) {
          if (size === 1) this.drawPixel(x + i, y + j, color);
          else this.fillRect(x + i * size, y + j * size, size, size, color);
        } else if (bg !== -1) {
          if (size === 1) this.drawPixel(x + i, y + j, bg);
          else this.fillRect(x + i * size, y + j * size, size, size, bg);
        }
      }
    }
    // Trailing spacing column
    if (bg !== -1) {
      if (size === 1) this.drawFastVLine(x + 5, y, 8, bg);
      else this.fillRect(x + 5 * size, y, size, 8 * size, bg);
    }
  }

  print(str) {
    const s = String(str);
    for (let i = 0; i < s.length; i++) {
      const ch = s[i];
      if (ch === '\n') {
        this.cursorX = 0;
        this.cursorY += this.textSize * 8;
      } else {
        this.drawChar(this.cursorX, this.cursorY, ch, this.textColor, this.textBg, this.textSize);
        this.cursorX += this.textSize * 6;
      }
    }
  }

  // Render 128x64 buffer with 4x integer scaling & OLED pixel matrix glow
  renderToCanvas(ctx, canvasW, canvasH) {
    ctx.fillStyle = "#000000";
    ctx.fillRect(0, 0, canvasW, canvasH);

    const scale = Math.floor(Math.min(canvasW / this.width, canvasH / this.height));
    const offsetX = Math.floor((canvasW - this.width * scale) / 2);
    const offsetY = Math.floor((canvasH - this.height * scale) / 2);

    for (let y = 0; y < this.height; y++) {
      for (let x = 0; x < this.width; x++) {
        const pixel = this.buffer[y * this.width + x];
        const px = offsetX + x * scale;
        const py = offsetY + y * scale;

        if (pixel) {
          // Lit OLED pixel: crisp pure white with sharp edges
          ctx.fillStyle = "#ffffff";
          ctx.fillRect(px, py, scale, scale);
        } else {
          // Unlit pixel: deep dark OLED matrix substrate
          ctx.fillStyle = "#030609";
          ctx.fillRect(px, py, scale, scale);
        }
      }
    }
  }
}

// --- 5. FIRMWARE DATA STRUCTURES (Mirroring src/types.h & src/config.h) ---
const MODE_SPLASH = -1;
const MODE_MAIN_MENU = 0;
const MODE_TX_REMOTE = 1;
const MODE_SIGNAL_SUBMENU = 2;
const MODE_SIGNAL_WAVEFORM = 3;
const MODE_SIGNAL_EDITOR = 4;
const MODE_RX_SNIFFER = 5;
const MODE_RX_DETAILS = 6;
const MODE_RAW_SNIFFER = 7;
const MODE_RAW_DETAILS = 8;
const MODE_FREQ_ANALYZER = 9;
const MODE_SPECTRUM = 10;
const MODE_CARRIER_GEN = 11;
const MODE_SETTINGS = 12;
const MODE_SYS_INFO = 13;
const MODE_24G_MENU = 14;
const MODE_BATTERY_INFO = 15;

let menu24gIndex = 0;
let batteryMv = 4050; // 4.05V
let batteryPct = 85;
let isCharging = false;

const FREQ_PRESETS = [433.92, 433.075, 433.42, 434.42, 434.775, 315.00, 868.35];
const FREQ_NAMES = ["433.92", "433.07", "433.42", "434.42", "434.77", "315.00", "868.35"];
const PA_NAMES = ["-30dB", "-10dB", "0dB", "+10dB"];
const SQUELCH_NAMES = ["-85dB", "-78dB", "-72dB", "-65dB"];
const SQUELCH_LEVELS = [-85, -78, -72, -65];

const SPECTRUM_BANDS = [
  { name: "433M", startFreq: 433.05, stepFreq: 0.054, labelRange: "433.05   433.9   434.8" },
  { name: "315M", startFreq: 314.00, stepFreq: 0.0625, labelRange: "314.0    315.0   316.0" },
  { name: "868M", startFreq: 867.50, stepFreq: 0.0625, labelRange: "867.5    868.5   869.5" }
];

// Stored Signals in Flash Memory
let savedSignals = [
  {
    name: "Gate Came 12",
    type: 0, // SIG_FIXED
    freq: 433.92,
    repeats: 6,
    protoName: "Came12",
    code: 0x0B35,
    bitLength: 12,
    pulseLength: 320,
    protocol: 2
  },
  {
    name: "Gate Came 24",
    type: 5, // SIG_CAME24
    freq: 433.92,
    repeats: 6,
    protoName: "Came24",
    code: 0x547A11,
    bitLength: 24,
    pulseLength: 320,
    protocol: 2
  },
  {
    name: "Nice Flo 12",
    type: 0, // SIG_FIXED
    freq: 433.92,
    repeats: 5,
    protoName: "NiceFLO",
    code: 0x01A2,
    bitLength: 12,
    pulseLength: 700,
    protocol: 3
  },
  {
    name: "Smart Relay",
    type: 3, // SIG_SMART_EV1527
    freq: 433.92,
    repeats: 5,
    protoName: "EV1527",
    code: 0x547A1,
    bitLength: 24,
    pulseLength: 320,
    protocol: 1,
    keeloqBtn: 1
  },
  {
    name: "DoorHan Gate",
    type: 9, // SIG_DOORHAN
    freq: 433.92,
    repeats: 5,
    protoName: "DoorHan",
    isKeeloq: true,
    keeloqSerial: 0x014B2C0,
    keeloqBtn: 1,
    keeloqEnc: 0x4B21,
    rawCount: 66,
    rawPulses: [400, 4000, 800, 400, 400, 800]
  },
  {
    name: "Somfy Blinds",
    type: 8, // SIG_SOMFY_RTS
    freq: 433.42,
    repeats: 4,
    protoName: "Somfy",
    code: 0x1A2B3C,
    bitLength: 56,
    pulseLength: 604,
    protocol: 1
  },
  {
    name: "Linear 10b",
    type: 7, // SIG_LINEAR10
    freq: 300.00,
    repeats: 6,
    protoName: "Linear",
    code: 0x02AB,
    bitLength: 10,
    pulseLength: 1000,
    protocol: 1
  },
  {
    name: "Socket ON",
    type: 0, // SIG_FIXED
    freq: 433.92,
    repeats: 5,
    protoName: "PT2262",
    code: 0x547A11,
    bitLength: 24,
    pulseLength: 350,
    protocol: 1
  },
  {
    name: "KeeLoq CarKey",
    type: 1, // SIG_RAW (KeeLoq)
    freq: 433.92,
    repeats: 5,
    protoName: "HCS301",
    isKeeloq: true,
    keeloqSerial: 0x02A58F0,
    keeloqBtn: 2,
    keeloqEnc: 0x3F81,
    rawCount: 66,
    rawPulses: [400, 800, 400, 400, 800, 400, 800, 800, 400, 800, 400, 400, 800, 400]
  }
];

// Active State Variables
let currentState = MODE_SPLASH;
let menuIndex = 0;
let txIndex = 0;
let sigSubMenuIdx = 0;
let editorMenuIdx = 0;
let editSignal = null;
let editSignalIndex = -1;

let currentFreqIdx = 0;
let currentPaIdx = 3;
let currentSquelchIdx = 1;
let currentBandIdx = 0;
let currentCarrierMode = 0; // 0=CW, 1=TONE, 2=JAM
let isTransmittingCarrier = false;

let bleEnabled = true;
let bleConnected = false;

// Captured packets
let capturedCode = 0x547A11;
let capturedBitLen = 24;
let capturedPulseLen = 320;
let capturedProto = 1;
let capturedRawPkt = {
  isKeeloq: true,
  keeloqSerial: 0x02A58F0,
  keeloqBtn: 2,
  keeloqEnc: 0x3F81,
  pulseCount: 66,
  protoName: "HCS301",
  pulses: [400, 800, 400, 400, 800, 400, 800, 800, 400, 800, 400, 400, 800, 400]
};

// Frequency Analyzer
let analyzerPeakFreq = 433.92;
let analyzerPeakRssi = -120;
let analyzerLocked = false;

// RSSI Spectrum
const SPECTRUM_CHANNELS = 32;
let spectrumBars = new Array(SPECTRUM_CHANNELS).fill(0);
let peakHold = new Array(SPECTRUM_CHANNELS).fill(0);

// Animation & Toast state
let rxAnimFrame = 0;
let toastMessage = null;
let toastTimeout = null;
let txAnimActive = false;

// Instantiate SSD1306 Emulator
const display = new SSD1306Device(128, 64);
const canvas = document.getElementById('oledCanvas');
const ctx = canvas ? canvas.getContext('2d') : null;

// --- 6. UI DRAWING PROCEDURES (Direct 1:1 C++ Implementation from src/ui.cpp) ---

function ui_show_toast(msg, duration = 800) {
  toastMessage = msg;
  if (toastTimeout) clearTimeout(toastTimeout);
  toastTimeout = setTimeout(() => {
    toastMessage = null;
    redrawActiveScreen();
  }, duration);
  redrawActiveScreen();
}

function ui_draw_header(title, badge = null) {
  display.fillRect(0, 0, 128, 10, 1);
  display.setTextColor(0, 1);
  display.setTextSize(1);

  // 1. Battery icon (115..126)
  display.drawRect(115, 2, 10, 6, 0);
  display.drawFastVLine(125, 3, 4, 0);
  const fillW = Math.max(0, Math.min(8, Math.round((batteryPct / 100) * 8)));
  if (fillW > 0) {
    display.fillRect(116, 3, fillW, 4, 0);
  }
  if (isCharging) {
    display.drawPixel(119, 4, 1);
    display.drawPixel(120, 3, 1);
  }

  // 2. Bluetooth icon (99..112)
  const btOn = bleConnected;
  if (btOn) {
    display.fillRect(99, 1, 13, 8, 0);
    display.setTextColor(1, 0);
    display.setCursor(100, 1);
    display.print("BT");
    display.setTextColor(0, 1);
  }

  const rightEdge = btOn ? 97 : 112;
  const badgeW = (badge && badge.length) ? (badge.length * 6) : 0;
  const badgeX = rightEdge - badgeW;

  if (badge && badge.length) {
    display.setCursor(badgeX, 1);
    display.print(badge);
  }

  const maxTitlePx = (badge && badge.length) ? (badgeX - 4) : (rightEdge - 2);
  const maxTitleChars = Math.max(1, Math.floor(maxTitlePx / 6));
  display.setCursor(2, 1);
  display.print(title.substring(0, maxTitleChars));
}

function ui_draw_footer(text) {
  display.drawFastHLine(0, 54, 128, 1);
  display.setTextColor(1, 0);
  display.setTextSize(1);
  display.setCursor(2, 56);
  display.print(text);
}

function ui_draw_toast(message) {
  display.fillRect(8, 20, 112, 22, 1);
  display.drawRect(9, 21, 110, 20, 0);
  display.setTextColor(0, 1);
  display.setTextSize(1);
  display.setCursor(14, 27);
  display.print(message);
}

function ui_draw_splash() {
  display.clearDisplay();
  display.drawBitmap(48, 6, ghostrf_bitmap_32x32, 32, 32, 1);
  display.setTextSize(2);
  display.setTextColor(1, 0);
  display.setCursor(22, 44);
  display.print("GHOSTRF");
  display.setTextSize(1);
}

function ui_draw_main_menu(idx) {
  display.clearDisplay();
  ui_draw_header("GHOSTRF", "C3");

  const items = [
    "1. Remote (TX)",
    "2. Sniffer (RX)",
    "3. RAW Sniffer",
    "4. Freq Analyzer",
    "5. RSSI Spectrum",
    "6. Carrier / Jam",
    "7. 2.4GHz Tools",
    "8. Battery Info",
    "9. Settings"
  ];

  const totalItems = 9;
  const start = Math.floor(idx / 4) * 4;
  const end = Math.min(start + 4, totalItems);

  for (let i = start; i < end; i++) {
    const y = 12 + (i - start) * 10;
    if (i === idx) {
      display.fillRect(0, y - 1, 128, 10, 1);
      display.setTextColor(0, 1);
      display.setCursor(2, y);
      display.print(">");
      display.setCursor(10, y);
      display.print(items[i]);
    } else {
      display.setTextColor(1, 0);
      display.setCursor(10, y);
      display.print(items[i]);
    }
  }

  ui_draw_footer("UP/DN:Nav  OK:Enter");
}

function ui_draw_24g_menu(idx) {
  display.clearDisplay();
  ui_draw_header("2.4G TOOLS", "WIFI/BT");

  const items = [
    "1. Wi-Fi Scanner",
    "2. 2.4G Channels",
    "3. Deauth Sniffer",
    "4. BLE Radar / Scan",
    "5. BLE Spammer",
    "6. Beacon Spam"
  ];

  const totalItems = 6;
  const start = Math.floor(idx / 4) * 4;
  const end = Math.min(start + 4, totalItems);

  for (let i = start; i < end; i++) {
    const y = 12 + (i - start) * 10;
    if (i === idx) {
      display.fillRect(0, y - 1, 128, 10, 1);
      display.setTextColor(0, 1);
      display.setCursor(2, y);
      display.print(">");
      display.setCursor(10, y);
      display.print(items[i]);
    } else {
      display.setTextColor(1, 0);
      display.setCursor(10, y);
      display.print(items[i]);
    }
  }

  ui_draw_footer("UP/DN:Nav  OK:Enter");
}

function ui_draw_tx_menu(sigList, sigCount, txIdx) {
  display.clearDisplay();
  ui_draw_header("REMOTE (TX)", "MEM");

  const totalItems = sigCount + 1;
  const start = Math.floor(txIdx / 3) * 3;
  const end = Math.min(start + 3, totalItems);

  for (let i = start; i < end; i++) {
    const y = 12 + (i - start) * 14;

    if (i === txIdx) {
      display.fillRect(0, y - 1, 128, 13, 1);
      display.setTextColor(0, 1);
      display.setCursor(2, y);
      display.print(">");
    } else {
      display.setTextColor(1, 0);
    }

    display.setCursor(10, y);
    if (i < sigCount) {
      display.print(sigList[i].name);

      display.setCursor(82, y);
      if (sigList[i].type === 0 || sigList[i].type === 3) {
        const hex = "0x" + ((sigList[i].code & 0xfffff) >>> 0).toString(16).toUpperCase().padStart(5, '0');
        display.print(hex);
      } else {
        if (sigList[i].isKeeloq) {
          display.print("[KLQ]");
        } else {
          display.print(`[${sigList[i].rawCount || 64}p]`);
        }
      }
    } else {
      display.print("[+] NEW SIGNAL");
    }
  }

  ui_draw_footer("OK:Select  Hold:Menu");
}

function ui_draw_signal_submenu(sig, subIdx) {
  display.clearDisplay();
  const badge = `[${sig.protoName || (sig.type === 0 ? "FIX" : "RAW")}]`;
  ui_draw_header(sig.name, badge);

  display.setTextColor(1, 0);
  display.setCursor(2, 13);
  if (sig.type === 3) {
    const btnNames = ["Btn 1", "Btn 2", "Btn 3", "Btn 4", "ALL"];
    const bIdx = Math.min(Math.max((sig.keeloqBtn || 1) - 1, 0), 4);
    const hex = "0x" + ((sig.code & 0xfffff) >>> 0).toString(16).toUpperCase().padStart(5, '0');
    display.print(`${sig.freq.toFixed(2)}M ${hex} [${btnNames[bIdx]}]`);
  } else if (sig.type === 0) {
    const hex = "0x" + ((sig.code & 0xfffff) >>> 0).toString(16).toUpperCase().padStart(5, '0');
    display.print(`${sig.freq.toFixed(2)}M ${hex} ${sig.repeats}x`);
  } else {
    if (sig.isKeeloq) {
      const sn = "0x" + ((sig.keeloqSerial & 0xfffffff) >>> 0).toString(16).toUpperCase().padStart(7, '0');
      display.print(`SN:${sn} ${sig.repeats}x`);
    } else {
      display.print(`${sig.freq.toFixed(2)}M ${sig.rawCount || 64}p ${sig.repeats}x`);
    }
  }

  display.drawFastHLine(0, 23, 128, 1);

  const actions = [];
  if (sig.type === 3) {
    const btnNames = ["Btn 1", "Btn 2", "Btn 3", "Btn 4", "ALL"];
    const bIdx = Math.min(Math.max((sig.keeloqBtn || 1) - 1, 0), 4);
    actions.push(`1. Transmit (${btnNames[bIdx]})`);
    actions.push(`2. Button: [${btnNames[bIdx]}]`);
    actions.push(`3. Repeat: [${sig.repeats}x]`);
    actions.push("4. Edit Signal");
    actions.push("5. Waveform / Info");
    actions.push("6. Delete Signal");
    actions.push("7. Back to List");
  } else {
    actions.push("1. Transmit (TX)");
    actions.push(`2. Repeat: [${sig.repeats}x]`);
    actions.push("3. Edit Signal");
    actions.push("4. Waveform / Info");
    actions.push("5. Delete Signal");
    actions.push("6. Back to List");
  }

  const start = Math.floor(subIdx / 3) * 3;
  const end = Math.min(start + 3, actions.length);

  for (let i = start; i < end; i++) {
    const y = 26 + (i - start) * 10;
    if (i === subIdx) {
      display.fillRect(0, y - 1, 128, 10, 1);
      display.setTextColor(0, 1);
      display.setCursor(2, y);
      display.print(">");
      display.setCursor(10, y);
      display.print(actions[i]);
    } else {
      display.setTextColor(1, 0);
      display.setCursor(10, y);
      display.print(actions[i]);
    }
  }

  ui_draw_footer("UP/DN:Nav  OK:Select");
}

function ui_draw_signal_waveform(sig) {
  display.clearDisplay();
  const badge = `[${sig.protoName || (sig.type === 0 ? "FIX" : "RAW")}]`;
  ui_draw_header(sig.name, badge);

  display.setTextColor(1, 0);
  display.setCursor(2, 12);
  if (sig.type === 3) {
    const btnNames = ["Btn 1", "Btn 2", "Btn 3", "Btn 4", "ALL"];
    const bIdx = Math.min(Math.max((sig.keeloqBtn || 1) - 1, 0), 4);
    const hex = "0x" + ((sig.code & 0xfffff) >>> 0).toString(16).toUpperCase().padStart(5, '0');
    display.print(`Addr:${hex}  ${btnNames[bIdx]}`);
  } else if (sig.type === 0) {
    const hex = "0x" + ((sig.code & 0xffffff) >>> 0).toString(16).toUpperCase().padStart(6, '0');
    display.print(`${hex} (P:${sig.protocol} Pw:${sig.pulseLength})`);
  } else {
    if (sig.isKeeloq) {
      const sn = "0x" + ((sig.keeloqSerial & 0xfffffff) >>> 0).toString(16).toUpperCase().padStart(7, '0');
      display.print(`SN:${sn} B:${sig.keeloqBtn}`);
    } else {
      display.print(`${sig.freq.toFixed(2)} MHz  ${sig.rawCount || 64} pulses`);
    }
  }

  // Mini-Oscilloscope Box (124x20 at x=2, y=22)
  display.drawRect(2, 22, 124, 20, 1);
  let x = 4;
  let lvl = true;
  if (sig.type === 1) { // SIG_RAW
    const pulses = sig.rawPulses || [300, 700, 300, 300, 700, 300, 700, 700, 300, 700];
    for (let i = 0; i < pulses.length && x < 122; i++) {
      const w = Math.min(Math.max(Math.floor((pulses[i] - 120) * 5 / 2380) + 2, 2), 7);
      const y = lvl ? 25 : 37;
      display.drawFastHLine(x, y, w, 1);
      display.drawFastVLine(x + w, 25, 13, 1);
      x += w;
      lvl = !lvl;
    }
  } else {
    const c = sig.code || 0x547A1;
    const bits = sig.bitLength || 24;
    for (let b = bits - 1; b >= 0 && x < 122; b--) {
      const bit = (c >> b) & 1;
      const w1 = bit ? 6 : 2;
      const w2 = bit ? 2 : 6;
      display.drawFastHLine(x, 25, w1, 1);
      display.drawFastVLine(x + w1, 25, 13, 1);
      display.drawFastHLine(x + w1, 37, w2, 1);
      display.drawFastVLine(x + w1 + w2, 25, 13, 1);
      x += (w1 + w2);
    }
  }

  display.setCursor(2, 45);
  display.print(`Repeats: ${sig.repeats}x  Power: +10dB`);
  ui_draw_footer("Any key: Back");
}

function ui_draw_signal_editor(sig, editIdx, isNew) {
  display.clearDisplay();
  const badge = `[${sig.protoName || "FIX"}]`;
  ui_draw_header(isNew ? "NEW SIGNAL" : "EDIT SIGNAL", badge);

  const btnNames = ["Btn 1", "Btn 2", "Btn 3", "Btn 4", "ALL"];
  const bIdx = Math.min(Math.max((sig.keeloqBtn || 1) - 1, 0), 4);
  const hex = "0x" + ((sig.code & 0xfffff) >>> 0).toString(16).toUpperCase().padStart(5, '0');

  const rows = [
    `Type: [${sig.protoName || "EV1527"}]`,
    `Name: [${sig.name}]`,
    `Freq: [${sig.freq.toFixed(2)}M]`,
    sig.type === 3 ? `Btn:  [${btnNames[bIdx]}]` : `Bits: [${sig.bitLength}b ${sig.pulseLength}us]`,
    `Addr: ${hex} [RAND]`,
    `Reps: [${sig.repeats}x]`,
    "[ TEST TRANSMIT ]",
    "[ SAVE SIGNAL ]",
    "[ CANCEL ]"
  ];

  const start = Math.floor(editIdx / 3) * 3;
  const end = Math.min(start + 3, 9);

  for (let i = start; i < end; i++) {
    const y = 12 + (i - start) * 14;
    if (i === editIdx) {
      display.fillRect(0, y - 1, 128, 13, 1);
      display.setTextColor(0, 1);
      display.setCursor(2, y);
      display.print(">");
    } else {
      display.setTextColor(1, 0);
    }
    display.setCursor(10, y);
    display.print(rows[i]);
  }

  if (editIdx === 7) ui_draw_footer("OK: Save to Flash");
  else if (editIdx === 6) ui_draw_footer("OK: Transmit Test");
  else if (editIdx === 8) ui_draw_footer("OK: Discard changes");
  else ui_draw_footer("OK: Edit  UP/DN: Nav");
}

function ui_draw_rx_screen(freqIdx) {
  display.clearDisplay();
  ui_draw_header("SNIFFER (RX)", FREQ_NAMES[freqIdx]);

  display.setTextColor(1, 0);
  display.setCursor(2, 13);
  display.print(`Freq: ${FREQ_NAMES[freqIdx]} MHz`);

  display.setCursor(2, 24);
  display.print("Mod:  ASK/OOK");

  display.setCursor(2, 35);
  display.print("Type: PT/EV1527");

  display.setCursor(2, 46);
  display.print("Listening...");

  const frames = ["-", "\\", "|", "/"];
  display.fillRect(74, 46, 24, 8, 0);
  display.setCursor(74, 46);
  display.print(`[ ${frames[rxAnimFrame % 4]} ]`);

  ui_draw_footer("Hold OK: Exit");
}

function ui_draw_rx_details(code, bitLength, pulseLength, protocol) {
  display.clearDisplay();
  const hex = "0x" + ((code & 0xffffff) >>> 0).toString(16).toUpperCase().padStart(6, '0');

  if (bitLength === 24) {
    ui_draw_header("EV1527 CAPTURED", "SMART");
    const addr = "0x" + (((code >> 4) & 0xfffff) >>> 0).toString(16).toUpperCase().padStart(5, '0');
    const btn = code & 0x0f;

    display.setTextColor(1, 0);
    display.setCursor(2, 12);
    display.print(`Addr: ${addr}  Btn: ${btn}`);

    display.setCursor(2, 23);
    display.print(`HEX: ${hex} (24b)`);

    display.setCursor(2, 34);
    display.print(`Proto:${protocol}  P:${pulseLength}us`);
  } else if (bitLength === 12) {
    ui_draw_header("12-BIT CAPTURED", "GATE");
    display.setTextColor(1, 0);
    display.setCursor(2, 12);
    display.print(`HEX: 0x${((code & 0xfff) >>> 0).toString(16).toUpperCase().padStart(3, '0')} (${code})`);

    display.setCursor(2, 23);
    display.print(`Bits: 12  Proto: ${protocol}`);

    display.setCursor(2, 34);
    display.print(`Pulse: ${pulseLength}us`);
  } else {
    ui_draw_header("FIXED CAPTURED!", "NEW");
    display.setTextColor(1, 0);
    display.setCursor(2, 12);
    display.print(`HEX: ${hex}`);

    display.setCursor(2, 23);
    display.print(`DEC: ${code} (${bitLength}b)`);

    display.setCursor(2, 34);
    display.print(`Proto:${protocol}  P:${pulseLength}us`);
  }

  display.setCursor(2, 45);
  display.print("[OK]Save [UP]TX [DN]X");
  ui_draw_footer("OK to save to NVS");
}

function ui_draw_raw_screen(freqIdx, squelchIdx) {
  display.clearDisplay();
  ui_draw_header("RAW SNIFFER", FREQ_NAMES[freqIdx]);

  display.setTextColor(1, 0);
  display.setCursor(2, 13);
  display.print(`Freq: ${FREQ_NAMES[freqIdx]} MHz`);

  display.setCursor(2, 23);
  display.print(`Squelch: ${SQUELCH_NAMES[squelchIdx]}`);

  const frames = ["-", "\\", "|", "/"];
  display.setCursor(2, 33);
  display.print(`Listening... [ ${frames[rxAnimFrame % 4]} ]`);

  // RSSI bar frame
  display.setCursor(2, 44);
  display.print("RSSI:");
  display.drawRect(34, 44, 90, 8, 1);

  // Live noise meter bar
  const rssi = -105 + Math.floor(Math.random() * 22);
  const barW = Math.max(0, Math.min(86, Math.floor((rssi + 115) * 86 / 90)));
  const thrX = Math.max(0, Math.min(86, Math.floor((SQUELCH_LEVELS[squelchIdx] + 115) * 86 / 90)));

  display.fillRect(36, 46, 86, 4, 0);
  if (barW > 0) display.fillRect(36, 46, barW, 4, 1);
  display.drawFastVLine(36 + thrX, 43, 8, 1);

  ui_draw_footer("Hold OK: Exit");
}

function ui_draw_raw_details(pkt) {
  display.clearDisplay();

  if (pkt.isKeeloq) {
    ui_draw_header("KEELOQ CAPTURED!", "HCS");
    display.setTextColor(1, 0);
    const sn = "0x" + ((pkt.keeloqSerial & 0xfffffff) >>> 0).toString(16).toUpperCase().padStart(7, '0');
    display.setCursor(2, 13);
    display.print(`SN: ${sn}`);

    const enc = "0x" + ((pkt.keeloqEnc & 0xffff) >>> 0).toString(16).toUpperCase().padStart(4, '0');
    display.setCursor(2, 24);
    display.print(`Btn: 0x${pkt.keeloqBtn.toString(16).toUpperCase()}  Enc: ${enc}`);

    display.setCursor(2, 35);
    display.print(`Pulses: ${pkt.pulseCount} (66-bit)`);

    display.setCursor(2, 45);
    display.print("[OK]Save [UP]TX [DN]X");
    ui_draw_footer("Save KeeLoq to NVS");
  } else {
    ui_draw_header("RAW CAPTURED!", `[${pkt.protoName}]`);
    display.setTextColor(1, 0);
    display.setCursor(2, 12);
    display.print(`Proto: ${pkt.protoName} (${pkt.pulseCount}p)`);

    // Mini-oscilloscope waveform
    display.drawRect(2, 22, 124, 18, 1);
    let x = 4;
    let lvl = true;
    for (let i = 0; i < pkt.pulses.length && x < 122; i++) {
      const w = Math.min(Math.max(Math.floor((pkt.pulses[i] - 120) * 6 / 2380) + 2, 2), 8);
      const y = lvl ? 25 : 35;
      display.drawFastHLine(x, y, w, 1);
      display.drawFastVLine(x + w, 25, 11, 1);
      x += w;
      lvl = !lvl;
    }

    display.setCursor(2, 44);
    display.print("[OK]Save [UP]TX [DN]X");
    ui_draw_footer("Save Signal to NVS");
  }
}

function ui_draw_freq_analyzer_screen() {
  display.clearDisplay();
  ui_draw_header("FREQ ANALYZER", "SCAN");

  display.fillRect(0, 11, 128, 42, 0);

  if (analyzerLocked) {
    display.fillRect(0, 11, 128, 11, 1);
    display.setTextColor(0, 1);
    display.setCursor(8, 13);
    display.print(">>> SIGNAL DETECTED! <<<");
  } else {
    display.setTextColor(1, 0);
    display.setCursor(2, 13);
    display.print("Press keyfob button...");
  }

  // Frequency readout
  display.setTextSize(2);
  display.setTextColor(1, 0);
  display.setCursor(8, 25);
  display.print(analyzerPeakFreq.toFixed(2));

  display.setTextSize(1);
  display.setCursor(84, 29);
  display.print("MHz");

  // RSSI bar
  display.setCursor(2, 44);
  display.print("RSSI:");
  display.drawRect(34, 44, 90, 8, 1);

  const barW = Math.max(0, Math.min(86, Math.floor((analyzerPeakRssi + 115) * 86 / 90)));
  if (barW > 0) display.fillRect(36, 46, barW, 4, 1);

  if (analyzerLocked) {
    ui_draw_footer("OK:Tune & Sniff  Hold:X");
  } else {
    ui_draw_footer(`Peak: ${analyzerPeakRssi}dBm`);
  }
}

function ui_draw_spectrum_screen(bandIdx) {
  display.clearDisplay();
  ui_draw_header("RSSI SPECTRUM", SPECTRUM_BANDS[bandIdx].name);

  // Background and range line
  display.drawFastHLine(0, 52, 128, 1);
  display.setTextColor(1, 0);
  display.setCursor(2, 55);
  display.print(SPECTRUM_BANDS[bandIdx].labelRange);

  let maxRssi = -120;
  for (let ch = 0; ch < SPECTRUM_CHANNELS; ch++) {
    let rssi = -105 + Math.floor(Math.sin((ch + Date.now() / 400) * 0.4) * 15);
    if (ch >= 14 && ch <= 17) rssi += 60 + Math.floor(Math.random() * 20); // Peak around center
    if (rssi > maxRssi) maxRssi = rssi;

    const barHeight = Math.max(0, Math.min(40, Math.floor((rssi + 110) * 40 / 95)));
    if (barHeight > peakHold[ch]) peakHold[ch] = barHeight;

    const x = ch * 4;
    const y = 51 - barHeight;

    if (barHeight > 0) display.fillRect(x, y, 3, barHeight, 1);
    if (peakHold[ch] > 1) display.drawFastHLine(x, 51 - peakHold[ch], 3, 1);
  }

  // Max peak badge in header
  display.fillRect(68, 0, 60, 10, 1);
  display.setTextColor(0, 1);
  display.setCursor(76, 1);
  display.print(`${maxRssi}dBm`);
}

function ui_draw_carrier_gen(freqIdx, paIdx, mode, isTx) {
  display.clearDisplay();
  ui_draw_header("CARRIER / JAM", "RF");

  display.setTextColor(1, 0);
  display.setCursor(2, 13);
  display.print(`Freq:  ${FREQ_NAMES[freqIdx]} MHz`);

  display.setCursor(2, 23);
  display.print(`Power: ${PA_NAMES[paIdx]}`);

  display.setCursor(2, 33);
  display.print("Mode:  ");
  if (mode === 0) display.print("CW Carrier");
  else if (mode === 1) display.print("1 kHz Tone");
  else display.print("Jammer Noise");

  display.setCursor(2, 44);
  display.print("State: ");
  if (isTx) {
    display.fillRect(44, 43, 80, 10, 1);
    display.setTextColor(0, 1);
    display.print("[ TX ACTIVE ]");
  } else {
    display.setTextColor(1, 0);
    display.print("DISABLED");
  }

  ui_draw_footer("OK:Toggle  UP/DN:Mode");
}

function ui_draw_settings_screen(enabled, connected) {
  display.clearDisplay();
  ui_draw_header("SETTINGS", enabled ? "BLE:ON" : "BLE:OFF");

  // Row 1: BLE enabled bar
  display.fillRect(0, 13, 128, 11, 1);
  display.setTextColor(0, 1);
  display.setCursor(4, 15);
  display.print(`> BLE: ${enabled ? "[ENABLED]" : "[DISABLED]"}`);

  // Row 2: Device Name
  display.setTextColor(1, 0);
  display.setCursor(4, 27);
  display.print("Dev: GHOSTRF");

  // Row 3: Current State
  display.setCursor(4, 39);
  if (!enabled) display.print("State: Inactive");
  else if (connected) display.print("State: [BT CONNECTED]");
  else display.print("State: [BT WAITING...]");

  ui_draw_footer("OK: Toggle ON/OFF");
}

function ui_draw_sys_info() {
  display.clearDisplay();
  ui_draw_header("ESP32-C3 INFO");

  display.setTextColor(1, 0);
  display.setCursor(2, 12);
  display.print("CPU: 160M  Temp:42C");

  display.setCursor(2, 23);
  display.print("RAM Free: 284 KB");

  display.setCursor(2, 34);
  const upSec = Math.floor(performance.now() / 1000);
  display.print(`Uptime: ${Math.floor(upSec / 60)}m ${upSec % 60}s`);

  display.setCursor(2, 45);
  display.print("CC1101: OK (Online)");

  ui_draw_footer("Any key: Back");
}



function ui_draw_battery_screen(mv, pct, charging) {
  display.clearDisplay();
  const badge = mv < 2000 ? "USB" : (mv > 4380 ? "OV!" : `${pct}%`);
  ui_draw_header("BATTERY", badge);
  display.setTextColor(1, 0);
  display.drawRect(12, 14, 98, 11, 1);
  display.fillRect(110, 16, 3, 7, 1);
  const barW = Math.max(0, Math.min(94, Math.round((pct / 100) * 94)));
  if (barW > 0) {
    display.fillRect(14, 16, barW, 7, 1);
  }
  display.setCursor(4, 29);
  if (mv < 2000) {
    display.print("Power: USB-C (5V)");
    display.setCursor(4, 41);
    display.print("Li-Po: Disconnected");
  } else if (mv > 4380) {
    display.print(`Volt: ${(mv / 1000).toFixed(2)}V (OVER!)`);
    display.setCursor(4, 41);
    display.print("ERR: Check Divider!");
  } else {
    display.print(`Volt: ${(mv / 1000).toFixed(2)}V (${mv}mV)`);
    display.setCursor(4, 41);
    if (charging) {
      display.print("Status: CHARGING");
    } else if (pct > 20) {
      display.print("Status: DISCHARGE OK");
    } else {
      display.print("Status: LOW BATTERY!");
    }
  }
  ui_draw_footer("OK: Back to Menu");
}

// --- 7. DISPATCHER & ACTIVE SCREEN RENDERER ---

function redrawActiveScreen() {
  if (currentState === MODE_SPLASH) {
    ui_draw_splash();
  } else if (currentState === MODE_MAIN_MENU) {
    ui_draw_main_menu(menuIndex);
  } else if (currentState === MODE_TX_REMOTE) {
    ui_draw_tx_menu(savedSignals, savedSignals.length, txIndex);
  } else if (currentState === MODE_SIGNAL_SUBMENU) {
    if (txIndex >= 0 && txIndex < savedSignals.length) {
      ui_draw_signal_submenu(savedSignals[txIndex], sigSubMenuIdx);
    }
  } else if (currentState === MODE_SIGNAL_WAVEFORM) {
    if (txIndex >= 0 && txIndex < savedSignals.length) {
      ui_draw_signal_waveform(savedSignals[txIndex]);
    }
  } else if (currentState === MODE_SIGNAL_EDITOR) {
    ui_draw_signal_editor(editSignal, editorMenuIdx, editSignalIndex === -1);
  } else if (currentState === MODE_RX_SNIFFER) {
    ui_draw_rx_screen(currentFreqIdx);
  } else if (currentState === MODE_RX_DETAILS) {
    ui_draw_rx_details(capturedCode, capturedBitLen, capturedPulseLen, capturedProto);
  } else if (currentState === MODE_RAW_SNIFFER) {
    ui_draw_raw_screen(currentFreqIdx, currentSquelchIdx);
  } else if (currentState === MODE_RAW_DETAILS) {
    ui_draw_raw_details(capturedRawPkt);
  } else if (currentState === MODE_FREQ_ANALYZER) {
    ui_draw_freq_analyzer_screen();
  } else if (currentState === MODE_SPECTRUM) {
    ui_draw_spectrum_screen(currentBandIdx);
  } else if (currentState === MODE_CARRIER_GEN) {
    ui_draw_carrier_gen(currentFreqIdx, currentPaIdx, currentCarrierMode, isTransmittingCarrier);
  } else if (currentState === MODE_SETTINGS) {
    ui_draw_settings_screen(bleEnabled, bleConnected);
  } else if (currentState === MODE_SYS_INFO) {
    ui_draw_sys_info();
  } else if (currentState === MODE_24G_MENU) {
    ui_draw_24g_menu(menu24gIndex);
  } else if (currentState === MODE_BATTERY_INFO) {
    ui_draw_battery_screen(batteryMv, batteryPct, isCharging);
  }

  // Transmitting banner overlay
  if (txAnimActive) {
    display.fillRect(0, 0, 128, 10, 1);
    display.setTextColor(0, 1);
    display.setCursor(10, 1);
    display.print(">>> TRANSMITTING <<<");
  }

  // Toast overlay
  if (toastMessage) {
    ui_draw_toast(toastMessage);
  }

  if (ctx && canvas) {
    display.renderToCanvas(ctx, canvas.width, canvas.height);
  }

  updateTelemetryPanel();
}

function updateTelemetryPanel() {
  const telFreq = document.getElementById('telFreq');
  const telSquelch = document.getElementById('telSquelch');
  const telPower = document.getElementById('telPower');
  const telCount = document.getElementById('telCount');
  const telCode = document.getElementById('telCode');
  const telKeeloq = document.getElementById('telKeeloq');
  const telState = document.getElementById('telState');

  if (telFreq) telFreq.textContent = `${FREQ_NAMES[currentFreqIdx]} MHz`;
  if (telSquelch) telSquelch.textContent = `${SQUELCH_NAMES[currentSquelchIdx]}`;
  if (telPower) telPower.textContent = `${PA_NAMES[currentPaIdx]} (${currentPaIdx === 3 ? '10 mW' : '1 mW'})`;
  if (telCount) telCount.textContent = `${savedSignals.length} / 20 слотов`;
  if (telCode) telCode.textContent = "0x" + ((capturedCode & 0xffffff) >>> 0).toString(16).toUpperCase().padStart(6, '0') + ` (${capturedBitLen}b)`;
  if (telKeeloq) telKeeloq.textContent = "SN: 0x" + ((capturedRawPkt.keeloqSerial & 0xfffffff) >>> 0).toString(16).toUpperCase().padStart(7, '0') + ` (B:${capturedRawPkt.keeloqBtn})`;

  const stateNames = [
    "MODE_MAIN_MENU", "MODE_TX_REMOTE", "MODE_SIGNAL_SUBMENU", "MODE_SIGNAL_WAVEFORM",
    "MODE_SIGNAL_EDITOR", "MODE_RX_SNIFFER", "MODE_RX_DETAILS", "MODE_RAW_SNIFFER",
    "MODE_RAW_DETAILS", "MODE_FREQ_ANALYZER", "MODE_SPECTRUM", "MODE_CARRIER_GEN",
    "MODE_SETTINGS", "MODE_SYS_INFO", "MODE_24G_MENU", "MODE_BATTERY_INFO"
  ];
  if (telState && currentState >= 0 && currentState < stateNames.length) {
    telState.textContent = stateNames[currentState];
  }
}

// --- 8. FIRMWARE BUTTON EVENT HANDLER (Mirroring src/main.cpp loop()) ---

function handleButtonEvent(evt) {
  playClickSound();

  if (currentState === MODE_SPLASH) {
    currentState = MODE_MAIN_MENU;
    redrawActiveScreen();
    return;
  }

  // GLOBAL OK HOLD (> 750ms): Return to Main Menu from ANY screen
  if (evt === 'HOLD') {
    if (isTransmittingCarrier) isTransmittingCarrier = false;
    currentState = MODE_MAIN_MENU;
    redrawActiveScreen();
    return;
  }

  switch (currentState) {
    case MODE_MAIN_MENU:
      if (evt === 'UP') {
        menuIndex = (menuIndex - 1 + 9) % 9;
      } else if (evt === 'DOWN') {
        menuIndex = (menuIndex + 1) % 9;
      } else if (evt === 'OK') {
        if (menuIndex === 0) {
          currentState = MODE_TX_REMOTE;
        } else if (menuIndex === 1) {
          currentState = MODE_RX_SNIFFER;
        } else if (menuIndex === 2) {
          currentState = MODE_RAW_SNIFFER;
        } else if (menuIndex === 3) {
          currentState = MODE_FREQ_ANALYZER;
          analyzerLocked = false;
        } else if (menuIndex === 4) {
          currentState = MODE_SPECTRUM;
          peakHold.fill(0);
        } else if (menuIndex === 5) {
          currentState = MODE_CARRIER_GEN;
          isTransmittingCarrier = false;
        } else if (menuIndex === 6) {
          currentState = MODE_24G_MENU;
          menu24gIndex = 0;
        } else if (menuIndex === 7) {
          currentState = MODE_BATTERY_INFO;
        } else if (menuIndex === 8) {
          currentState = MODE_SETTINGS;
        }
      }
      break;

    case MODE_24G_MENU:
      if (evt === 'UP') {
        menu24gIndex = (menu24gIndex - 1 + 6) % 6;
      } else if (evt === 'DOWN') {
        menu24gIndex = (menu24gIndex + 1) % 6;
      } else if (evt === 'OK') {
        ui_draw_toast("2.4G ACTIVE (MCU)");
        setTimeout(() => redrawActiveScreen(), 800);
      }
      break;

    case MODE_TX_REMOTE: {
      const maxTx = savedSignals.length + 1; // Signals + [+] NEW SIGNAL
      if (evt === 'UP') {
        txIndex = (txIndex - 1 + maxTx) % maxTx;
      } else if (evt === 'DOWN') {
        txIndex = (txIndex + 1) % maxTx;
      } else if (evt === 'OK') {
        if (txIndex < savedSignals.length) {
          currentState = MODE_SIGNAL_SUBMENU;
          sigSubMenuIdx = 0;
        } else {
          // [+] ADD NEW SIGNAL
          editSignalIndex = -1;
          editorMenuIdx = 0;
          editSignal = {
            name: `Sig_${(savedSignals.length + 1).toString().padStart(2, '0')}`,
            type: 3, // SIG_SMART_EV1527
            freq: 433.92,
            repeats: 5,
            protoName: "EV1527",
            code: Math.floor(Math.random() * 0xfffff),
            bitLength: 24,
            pulseLength: 320,
            protocol: 1,
            keeloqBtn: 1
          };
          currentState = MODE_SIGNAL_EDITOR;
        }
      }
      break;
    }

    case MODE_SIGNAL_SUBMENU: {
      const sig = savedSignals[txIndex];
      const totalActions = sig.type === 3 ? 7 : 6;

      if (evt === 'UP') {
        sigSubMenuIdx = (sigSubMenuIdx - 1 + totalActions) % totalActions;
      } else if (evt === 'DOWN') {
        sigSubMenuIdx = (sigSubMenuIdx + 1) % totalActions;
      } else if (evt === 'OK') {
        if (sig.type === 3) {
          if (sigSubMenuIdx === 0) {
            // Transmit
            playChirpSound();
            txAnimActive = true;
            redrawActiveScreen();
            setTimeout(() => {
              txAnimActive = false;
              redrawActiveScreen();
            }, 350);
          } else if (sigSubMenuIdx === 1) {
            // Button cycle
            sig.keeloqBtn = (sig.keeloqBtn % 5) + 1;
          } else if (sigSubMenuIdx === 2) {
            // Repeats toggle
            if (sig.repeats <= 1) sig.repeats = 3;
            else if (sig.repeats <= 3) sig.repeats = 5;
            else if (sig.repeats <= 5) sig.repeats = 10;
            else sig.repeats = 1;
          } else if (sigSubMenuIdx === 3) {
            // Edit
            editSignal = JSON.parse(JSON.stringify(sig));
            editSignalIndex = txIndex;
            editorMenuIdx = 0;
            currentState = MODE_SIGNAL_EDITOR;
          } else if (sigSubMenuIdx === 4) {
            // Waveform
            currentState = MODE_SIGNAL_WAVEFORM;
          } else if (sigSubMenuIdx === 5) {
            // Delete
            savedSignals.splice(txIndex, 1);
            if (txIndex >= savedSignals.length && savedSignals.length > 0) txIndex = savedSignals.length - 1;
            ui_show_toast("SIGNAL DELETED!");
            currentState = MODE_TX_REMOTE;
          } else if (sigSubMenuIdx === 6) {
            // Back
            currentState = MODE_TX_REMOTE;
          }
        } else {
          if (sigSubMenuIdx === 0) {
            // Transmit
            playChirpSound();
            txAnimActive = true;
            redrawActiveScreen();
            setTimeout(() => {
              txAnimActive = false;
              redrawActiveScreen();
            }, 350);
          } else if (sigSubMenuIdx === 1) {
            // Repeats
            if (sig.repeats <= 1) sig.repeats = 3;
            else if (sig.repeats <= 3) sig.repeats = 5;
            else if (sig.repeats <= 5) sig.repeats = 10;
            else sig.repeats = 1;
          } else if (sigSubMenuIdx === 2) {
            // Edit
            editSignal = JSON.parse(JSON.stringify(sig));
            editSignalIndex = txIndex;
            editorMenuIdx = 0;
            currentState = MODE_SIGNAL_EDITOR;
          } else if (sigSubMenuIdx === 3) {
            // Waveform
            currentState = MODE_SIGNAL_WAVEFORM;
          } else if (sigSubMenuIdx === 4) {
            // Delete
            savedSignals.splice(txIndex, 1);
            if (txIndex >= savedSignals.length && savedSignals.length > 0) txIndex = savedSignals.length - 1;
            ui_show_toast("SIGNAL DELETED!");
            currentState = MODE_TX_REMOTE;
          } else if (sigSubMenuIdx === 5) {
            // Back
            currentState = MODE_TX_REMOTE;
          }
        }
      }
      break;
    }

    case MODE_SIGNAL_WAVEFORM:
      currentState = MODE_SIGNAL_SUBMENU;
      break;

    case MODE_SIGNAL_EDITOR: {
      if (evt === 'UP') {
        editorMenuIdx = (editorMenuIdx - 1 + 9) % 9;
      } else if (evt === 'DOWN') {
        editorMenuIdx = (editorMenuIdx + 1) % 9;
      } else if (evt === 'OK') {
        if (editorMenuIdx === 0) {
          // Cycle type
          if (editSignal.protoName === "EV1527") {
            editSignal.protoName = "PT2262";
            editSignal.type = 0;
            editSignal.bitLength = 24;
            editSignal.pulseLength = 350;
            editSignal.protocol = 1;
          } else if (editSignal.protoName === "PT2262") {
            editSignal.protoName = "Came";
            editSignal.type = 0;
            editSignal.bitLength = 12;
            editSignal.pulseLength = 320;
            editSignal.protocol = 2;
            editSignal.code &= 0xfff;
          } else if (editSignal.protoName === "Came") {
            editSignal.protoName = "Nice";
            editSignal.type = 0;
            editSignal.bitLength = 12;
            editSignal.pulseLength = 700;
            editSignal.protocol = 3;
            editSignal.code &= 0xfff;
          } else {
            editSignal.protoName = "EV1527";
            editSignal.type = 3;
            editSignal.bitLength = 24;
            editSignal.pulseLength = 320;
            editSignal.protocol = 1;
            editSignal.keeloqBtn = 1;
          }
        } else if (editorMenuIdx === 1) {
          // Cycle name
          const names = ["Gate", "Barrier", "CarKey", "Light", "Alarm", "Remote", "Socket", "Door", "Sig_01", "Sig_02"];
          let nIdx = 0;
          for (let p = 0; p < names.length; p++) {
            if (editSignal.name.startsWith(names[p])) {
              nIdx = (p + 1) % names.length;
              break;
            }
          }
          editSignal.name = names[nIdx];
        } else if (editorMenuIdx === 2) {
          // Cycle Freq
          let fIdx = 0;
          for (let f = 0; f < FREQ_PRESETS.length; f++) {
            if (Math.abs(editSignal.freq - FREQ_PRESETS[f]) < 0.02) {
              fIdx = (f + 1) % FREQ_PRESETS.length;
              break;
            }
          }
          editSignal.freq = FREQ_PRESETS[fIdx];
        } else if (editorMenuIdx === 3) {
          // Btn / Bits
          if (editSignal.type === 3) {
            editSignal.keeloqBtn = (editSignal.keeloqBtn % 5) + 1;
          } else {
            if (editSignal.pulseLength <= 320) editSignal.pulseLength = 350;
            else if (editSignal.pulseLength <= 350) editSignal.pulseLength = 450;
            else editSignal.pulseLength = 320;
          }
        } else if (editorMenuIdx === 4) {
          // Random Code
          const mask = editSignal.bitLength === 12 ? 0xfff : 0xfffff;
          editSignal.code = Math.floor(Math.random() * mask);
        } else if (editorMenuIdx === 5) {
          // Repeats
          if (editSignal.repeats <= 1) editSignal.repeats = 3;
          else if (editSignal.repeats <= 3) editSignal.repeats = 5;
          else if (editSignal.repeats <= 5) editSignal.repeats = 7;
          else if (editSignal.repeats <= 7) editSignal.repeats = 10;
          else editSignal.repeats = 1;
        } else if (editorMenuIdx === 6) {
          // Test Transmit
          playChirpSound();
          txAnimActive = true;
          redrawActiveScreen();
          setTimeout(() => {
            txAnimActive = false;
            redrawActiveScreen();
          }, 350);
        } else if (editorMenuIdx === 7) {
          // Save
          if (editSignalIndex === -1) {
            savedSignals.push(editSignal);
            txIndex = savedSignals.length - 1;
          } else {
            savedSignals[editSignalIndex] = editSignal;
            txIndex = editSignalIndex;
          }
          ui_show_toast("SIGNAL SAVED!");
          currentState = MODE_TX_REMOTE;
        } else if (editorMenuIdx === 8) {
          // Cancel
          currentState = MODE_TX_REMOTE;
        }
      }
      break;
    }

    case MODE_RX_DETAILS:
      if (evt === 'UP') {
        playChirpSound();
        txAnimActive = true;
        redrawActiveScreen();
        setTimeout(() => {
          txAnimActive = false;
          redrawActiveScreen();
        }, 350);
      } else if (evt === 'DOWN') {
        currentState = MODE_RX_SNIFFER;
      } else if (evt === 'OK') {
        savedSignals.push({
          name: `Smart_${savedSignals.length + 1}`,
          type: 3,
          freq: FREQ_PRESETS[currentFreqIdx],
          repeats: 5,
          protoName: "EV1527",
          code: capturedCode,
          bitLength: capturedBitLen,
          pulseLength: capturedPulseLen,
          protocol: capturedProto,
          keeloqBtn: (capturedCode & 0x0f) || 1
        });
        ui_show_toast("SAVED TO NVS!");
        currentState = MODE_TX_REMOTE;
        txIndex = savedSignals.length - 1;
      }
      break;

    case MODE_RAW_DETAILS:
      if (evt === 'UP') {
        playChirpSound();
        txAnimActive = true;
        redrawActiveScreen();
        setTimeout(() => {
          txAnimActive = false;
          redrawActiveScreen();
        }, 350);
      } else if (evt === 'DOWN') {
        currentState = MODE_RAW_SNIFFER;
      } else if (evt === 'OK') {
        savedSignals.push({
          name: `KLQ_${savedSignals.length + 1}`,
          type: 1,
          freq: FREQ_PRESETS[currentFreqIdx],
          repeats: 5,
          protoName: "HCS301",
          isKeeloq: true,
          keeloqSerial: capturedRawPkt.keeloqSerial,
          keeloqBtn: capturedRawPkt.keeloqBtn,
          keeloqEnc: capturedRawPkt.keeloqEnc,
          rawCount: capturedRawPkt.pulseCount,
          rawPulses: capturedRawPkt.pulses
        });
        ui_show_toast("SAVED TO NVS!");
        currentState = MODE_TX_REMOTE;
        txIndex = savedSignals.length - 1;
      }
      break;

    case MODE_FREQ_ANALYZER:
      if (evt === 'UP' || evt === 'DOWN') {
        analyzerPeakRssi = -120;
        analyzerLocked = false;
      } else if (evt === 'OK') {
        ui_show_toast("TUNED TO FREQ!");
        currentState = MODE_RAW_SNIFFER;
      }
      break;

    case MODE_SPECTRUM:
      if (evt === 'UP') {
        currentBandIdx = (currentBandIdx - 1 + SPECTRUM_BANDS.length) % SPECTRUM_BANDS.length;
        peakHold.fill(0);
      } else if (evt === 'DOWN') {
        currentBandIdx = (currentBandIdx + 1) % SPECTRUM_BANDS.length;
        peakHold.fill(0);
      } else if (evt === 'OK') {
        peakHold.fill(0);
      }
      break;

    case MODE_CARRIER_GEN:
      if (evt === 'UP') {
        currentCarrierMode = (currentCarrierMode - 1 + 3) % 3;
      } else if (evt === 'DOWN') {
        currentCarrierMode = (currentCarrierMode + 1) % 3;
      } else if (evt === 'OK') {
        isTransmittingCarrier = !isTransmittingCarrier;
        if (isTransmittingCarrier) {
          playToneSound(currentCarrierMode === 1 ? 1000 : 600, 0.4);
        }
      }
      break;

    case MODE_SETTINGS:
      if (evt === 'OK') {
        bleEnabled = !bleEnabled;
        ui_show_toast(bleEnabled ? "BLE ENABLED" : "BLE DISABLED");
      }
      break;


    case MODE_BATTERY_INFO:
      if (evt === 'OK') {
        currentState = MODE_MAIN_MENU;
      }
      break;

    case MODE_SYS_INFO:
      currentState = MODE_SETTINGS;
      break;
  }

  redrawActiveScreen();
}

// --- 9. PERIODIC BACKGROUND TIMERS (Mirroring loop() in src/main.cpp) ---

setInterval(() => {
  rxAnimFrame++;
  if (currentState === MODE_RX_SNIFFER || currentState === MODE_RAW_SNIFFER) {
    redrawActiveScreen();
  }
}, 250);

setInterval(() => {
  if (currentState === MODE_SPECTRUM) {
    // Peak decay
    for (let i = 0; i < SPECTRUM_CHANNELS; i++) {
      if (peakHold[i] > 0) peakHold[i]--;
    }
    redrawActiveScreen();
  }
}, 1200);

setInterval(() => {
  if (currentState === MODE_FREQ_ANALYZER && !analyzerLocked) {
    // Random signal detection simulation
    if (Math.random() > 0.65) {
      analyzerPeakRssi = -42 - Math.floor(Math.random() * 18);
      analyzerPeakFreq = 433.92;
      analyzerLocked = true;
      playChirpSound();
    } else {
      analyzerPeakRssi = -115 + Math.floor(Math.random() * 20);
    }
    redrawActiveScreen();
  }
}, 300);

// Auto-boot sequence: Splash for 600ms, then main menu
setTimeout(() => {
  if (currentState === MODE_SPLASH) {
    currentState = MODE_MAIN_MENU;
    redrawActiveScreen();
  }
}, 600);

// --- 10. DOM EVENT BINDINGS (D-PAD & KEYBOARD) ---

const btnUp = document.getElementById('btnUp');
const btnDown = document.getElementById('btnDown');
const btnOk = document.getElementById('btnOk');
const btnHoldShortcut = document.getElementById('btnHoldShortcut');

if (btnUp) btnUp.addEventListener('click', () => handleButtonEvent('UP'));
if (btnDown) btnDown.addEventListener('click', () => handleButtonEvent('DOWN'));
if (btnOk) btnOk.addEventListener('click', () => handleButtonEvent('OK'));
if (btnHoldShortcut) btnHoldShortcut.addEventListener('click', () => handleButtonEvent('HOLD'));

// Long hold simulation on OK button
let okPressTimer = null;
if (btnOk) {
  btnOk.addEventListener('mousedown', () => {
    okPressTimer = setTimeout(() => {
      handleButtonEvent('HOLD');
      okPressTimer = null;
    }, 750);
  });
  btnOk.addEventListener('mouseup', () => {
    if (okPressTimer) {
      clearTimeout(okPressTimer);
      okPressTimer = null;
    }
  });
}

// Quick Mode Jump Buttons
document.querySelectorAll('.btn-mode-quick').forEach(btn => {
  btn.addEventListener('click', () => {
    const mode = btn.getAttribute('data-mode');
    if (mode !== null) {
      currentState = parseInt(mode);
      redrawActiveScreen();
    }
  });
});

// Trigger simulated RX packet
const btnQuickPacket = document.getElementById('btnQuickPacket');
if (btnQuickPacket) {
  btnQuickPacket.addEventListener('click', () => {
    playChirpSound();
    if (currentState === MODE_RAW_SNIFFER) {
      capturedRawPkt = {
        isKeeloq: true,
        keeloqSerial: 0x02a58f0,
        keeloqBtn: Math.floor(Math.random() * 4) + 1,
        keeloqEnc: Math.floor(Math.random() * 0xffff),
        pulseCount: 66,
        protoName: "HCS301",
        pulses: [400, 800, 400, 400, 800, 400, 800, 800, 400, 800, 400, 400, 800, 400]
      };
      currentState = MODE_RAW_DETAILS;
    } else {
      capturedCode = 0x547a10 | (Math.floor(Math.random() * 15) + 1);
      capturedBitLen = 24;
      capturedPulseLen = 320;
      capturedProto = 1;
      currentState = MODE_RX_DETAILS;
    }
    redrawActiveScreen();
  });
}

// Reboot button
const btnReboot = document.getElementById('btnReboot');
if (btnReboot) {
  btnReboot.addEventListener('click', () => {
    playClickSound(600, 0.05);
    currentState = MODE_SPLASH;
    redrawActiveScreen();
    setTimeout(() => {
      currentState = MODE_MAIN_MENU;
      redrawActiveScreen();
    }, 600);
  });
}

// Physical Keyboard mappings: Up, Down, Enter, Escape, P (Packet)
window.addEventListener('keydown', (e) => {
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
  if (e.key === 'ArrowUp') {
    e.preventDefault();
    handleButtonEvent('UP');
  } else if (e.key === 'ArrowDown') {
    e.preventDefault();
    handleButtonEvent('DOWN');
  } else if (e.key === 'Enter') {
    e.preventDefault();
    handleButtonEvent('OK');
  } else if (e.key === 'Escape' || e.key === 'Backspace') {
    e.preventDefault();
    handleButtonEvent('HOLD');
  } else if (e.key === 'p' || e.key === 'P') {
    btnQuickPacket?.click();
  }
});

// --- 11. WEBSERIAL TERMINAL HANDLER ---
const btnWebSerialConnect = document.getElementById('btnWebSerialConnect');
if (btnWebSerialConnect) {
  btnWebSerialConnect.addEventListener('click', async () => {
    playClickSound(1000, 0.03);
    const termLog = document.getElementById('termLog');
    if ('serial' in navigator) {
      try {
        termLog.innerHTML += '<div class="term-line highlight">[USB] Запрос COM-порта у пользователя...</div>';
        const port = await navigator.serial.requestPort();
        await port.open({ baudRate: 115200 });
        termLog.innerHTML += '<div class="term-line highlight">[USB] Порт успешно открыт на 115200 бод!</div>';
        termLog.innerHTML += '<div class="term-line">[MCU] Отклик ESP32-C3: OK (Chip Rev v0.4)</div>';
        btnWebSerialConnect.textContent = '[ COM: ОНЛАЙН ]';
        btnWebSerialConnect.style.backgroundColor = '#ffffff';
        btnWebSerialConnect.style.color = '#000000';
      } catch (err) {
        termLog.innerHTML += `<div class="term-line">[USB] Ошибка: ${err.message}</div>`;
      }
    } else {
      termLog.innerHTML += '<div class="term-line">[USB] WebSerial API не поддерживается в данном браузере.</div>';
    }
  });
}

// Initial draw
redrawActiveScreen();
