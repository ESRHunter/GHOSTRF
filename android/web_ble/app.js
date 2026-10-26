// Web Bluetooth Nordic UART Service UUIDs
const SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const RX_CHAR_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e'; // Browser -> ESP32 (Write)
const TX_CHAR_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'; // ESP32 -> Browser (Notify)

let bleDevice = null;
let rxCharacteristic = null;
let txCharacteristic = null;
let isConnected = false;
let rxBuffer = '';

// OLED canvas setup
const canvas = document.getElementById('oledCanvas');
const ctx = canvas.getContext('2d');
const oledBuffer = new Uint8Array(1024);
let isInverted = false;
let hasGrid = true;
let isStreaming = true;
let lastScreenFrameTime = 0;

const statusBadge = document.getElementById('statusBadge');
const btnConnect = document.getElementById('btnConnect');
const logConsole = document.getElementById('logConsole');

function log(msg, isError = false) {
    const time = new Date().toTimeString().split(' ')[0];
    const line = document.createElement('div');
    line.style.color = isError ? '#ffffff' : '#aaaaaa';
    line.textContent = `[${time}] ${msg}`;
    logConsole.appendChild(line);
    logConsole.scrollTop = logConsole.scrollHeight;
}

// Draw SSD1306 128x64 buffer onto canvas
function drawOled() {
    const w = canvas.width;
    const h = canvas.height;
    ctx.fillStyle = isInverted ? '#ffffff' : '#000000';
    ctx.fillRect(0, 0, w, h);

    const pixelW = w / 128;
    const pixelH = h / 64;
    ctx.fillStyle = isInverted ? '#000000' : '#ffffff';

    for (let page = 0; page < 8; page++) {
        const pageOffset = page * 128;
        for (let x = 0; x < 128; x++) {
            const b = oledBuffer[pageOffset + x];
            for (let bit = 0; bit < 8; bit++) {
                const isPixelOn = (b & (1 << bit)) !== 0;
                if (isPixelOn) {
                    const px = x * pixelW;
                    const py = (page * 8 + bit) * pixelH;
                    if (hasGrid) {
                        ctx.fillRect(px + 0.5, py + 0.5, pixelW - 0.5, pixelH - 0.5);
                    } else {
                        ctx.fillRect(px, py, pixelW, pixelH);
                    }
                }
            }
        }
    }
}

function updateFrameFromHex(hex) {
    if (hex.length < 2048) return;
    for (let i = 0; i < 1024; i++) {
        oledBuffer[i] = parseInt(hex.substr(i * 2, 2), 16);
    }
    drawOled();
}

// Initial display draw
drawOled();

// Connect BLE
async function connectBle() {
    if (!navigator.bluetooth) {
        log('Web Bluetooth не поддерживается этим браузером! Используйте Google Chrome на Android.', true);
        alert('Web Bluetooth доступен в Google Chrome на Android или десктопе с поддержкой Bluetooth.');
        return;
    }

    try {
        log('Поиск устройства GHOSTRF...');
        bleDevice = await navigator.bluetooth.requestDevice({
            filters: [
                { namePrefix: 'GHOSTRF' },
                { namePrefix: 'RF-HUB' },
                { namePrefix: 'ESP32' }
            ],
            optionalServices: [SERVICE_UUID]
        });

        bleDevice.addEventListener('gattserverdisconnected', onDisconnected);

        log(`Подключение к ${bleDevice.name}...`);
        const server = await bleDevice.gatt.connect();

        log('Получение сервиса Nordic UART...');
        const service = await server.getPrimaryService(SERVICE_UUID);

        rxCharacteristic = await service.getCharacteristic(RX_CHAR_UUID);
        txCharacteristic = await service.getCharacteristic(TX_CHAR_UUID);

        await txCharacteristic.startNotifications();
        txCharacteristic.addEventListener('characteristicvaluechanged', onCharacteristicChanged);

        isConnected = true;
        statusBadge.textContent = `[ ${bleDevice.name} ]`;
        statusBadge.classList.add('connected');
        btnConnect.textContent = '[ ОТКЛЮЧИТЬ ]';
        log('[BLE OK] Успешно подключено к ' + bleDevice.name);

        sendCommand('PING');
        sendCommand('STREAM_SCREEN 1');
        setTimeout(() => sendCommand('GET_SCREEN'), 100);
    } catch (err) {
        log('[ERR] ' + err.message, true);
    }
}

function disconnectBle() {
    if (bleDevice && bleDevice.gatt.connected) {
        bleDevice.gatt.disconnect();
    }
}

function onDisconnected() {
    isConnected = false;
    statusBadge.textContent = '[ BLE: ОТКЛЮЧЕНО ]';
    statusBadge.classList.remove('connected');
    btnConnect.textContent = '[ ПОДКЛЮЧИТЬ ]';
    log('[BLE] Устройство отключено.');
}

function onCharacteristicChanged(event) {
    const value = event.target.value;
    const decoder = new TextDecoder('utf-8');
    const chunk = decoder.decode(value);

    rxBuffer += chunk;
    let newlineIdx = rxBuffer.indexOf('\n');
    while (newlineIdx !== -1) {
        const line = rxBuffer.substring(0, newlineIdx).trim();
        rxBuffer = rxBuffer.substring(newlineIdx + 1);
        if (line.length > 0) {
            handleIncomingLine(line);
        }
        newlineIdx = rxBuffer.indexOf('\n');
    }
}

function handleIncomingLine(line) {
    if (!line.startsWith('{')) return;
    try {
        const json = JSON.parse(line);
        if (json.cmd === 'SCREEN' && json.hex) {
            lastScreenFrameTime = Date.now();
            updateFrameFromHex(json.hex);
        } else if (json.event === 'CAPTURED_FIXED') {
            log(`[RX FIXED] ${json.codeHex} (${json.code}) | ${json.bitLen} бит, TE=${json.pulseLen} мкс | ${json.freq} МГц | RSSI: ${json.rssi} dBm`);
        } else if (json.event === 'CAPTURED_RAW') {
            const name = json.isKeeloq ? `KeeLoq (SN:${json.serial}, B:${json.btn})` : json.proto;
            log(`[RX RAW] ${name} | ${json.count} имп. | ${json.freq} МГц | RSSI: ${json.rssi} dBm`);
        } else if (json.event === 'DEVICE_READY') {
            log(`[READY] Устройство ${json.device} готово!`);
            sendCommand('STREAM_SCREEN 1');
            setTimeout(() => sendCommand('GET_SCREEN'), 100);
        } else if (json.cmd === 'PONG') {
            log(`[PONG] ${json.device} (CC1101: ${json.cc1101 ? 'OK' : 'ERR'}) | Freq: ${json.freq} МГц`);
        }
    } catch (e) {}
}

async function sendCommand(cmd) {
    if (!isConnected || !rxCharacteristic) {
        log('[WARN] Устройство не подключено!', true);
        return;
    }
    try {
        const encoder = new TextEncoder();
        const data = encoder.encode(cmd.trim() + '\n');
        await rxCharacteristic.writeValueWithoutResponse(data);
    } catch (e) {
        log('[ERR TX] ' + e.message, true);
    }
}

// Watchdog: если подключены, стриминг активен, но кадров нет > 2.5 сек, запрашиваем кадр
setInterval(() => {
    if (isConnected && isStreaming) {
        if (Date.now() - lastScreenFrameTime > 2500) {
            sendCommand('STREAM_SCREEN 1');
            sendCommand('GET_SCREEN');
        }
    }
}, 2000);

// UI Event Listeners
btnConnect.addEventListener('click', () => {
    if (isConnected) {
        disconnectBle();
    } else {
        connectBle();
    }
});

document.getElementById('btnStream').addEventListener('click', () => {
    isStreaming = !isStreaming;
    document.getElementById('btnStream').textContent = isStreaming ? '[ СТРИМ: ВКЛ ]' : '[ СТРИМ: ВЫКЛ ]';
    sendCommand(isStreaming ? 'STREAM_SCREEN 1' : 'STREAM_SCREEN 0');
    if (isStreaming) {
        sendCommand('GET_SCREEN');
    }
});

document.getElementById('btnInvert').addEventListener('click', () => {
    isInverted = !isInverted;
    drawOled();
});

document.getElementById('btnGrid').addEventListener('click', () => {
    hasGrid = !hasGrid;
    drawOled();
});

// D-pad controls (после нажатия сразу запрашиваем обновленный кадр)
function sendRemote(btn) {
    sendCommand(`REMOTE_BTN ${btn}`);
    setTimeout(() => sendCommand('GET_SCREEN'), 80);
}

document.getElementById('btnUp').addEventListener('click', () => sendRemote('UP'));
document.getElementById('btnOk').addEventListener('click', () => sendRemote('OK'));
document.getElementById('btnDown').addEventListener('click', () => sendRemote('DOWN'));
document.getElementById('btnMenu').addEventListener('click', () => sendRemote('OK_HOLD'));

// Quick Mode Selection (GOTO_MODE on ESP32)
document.querySelectorAll('.btn-mode').forEach(btn => {
    btn.addEventListener('click', () => {
        const mode = btn.getAttribute('data-mode');
        sendCommand(`GOTO_MODE ${mode}`);
        setTimeout(() => sendCommand('GET_SCREEN'), 100);
        log(`[MODE] Запрос переключения режима: ${mode}`);
    });
});

// Apply Radio Config
document.getElementById('btnApplyRadio').addEventListener('click', () => {
    const f = document.getElementById('selFreq').value;
    const p = document.getElementById('selPa').value;
    const sq = document.getElementById('selSquelch').value;
    sendCommand(`SET_CONFIG ${f} ${p} ${sq}`);
    log(`[CFG] Применены параметры: FreqIdx=${f}, PAIdx=${p}, SquelchIdx=${sq}`);
});
