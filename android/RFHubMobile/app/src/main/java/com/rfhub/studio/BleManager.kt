package com.rfhub.studio

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import org.json.JSONObject
import java.util.*

@SuppressLint("MissingPermission")
class BleManager(private val context: Context) {

    companion object {
        val SERVICE_UUID: UUID = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
        val RX_CHAR_UUID: UUID = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E") // App -> ESP32 (Write)
        val TX_CHAR_UUID: UUID = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E") // ESP32 -> App (Notify)
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }

    interface Listener {
        fun onConnectionStateChange(connected: Boolean, deviceName: String)
        fun onScanStateChange(scanning: Boolean)
        fun onDeviceFound(device: BluetoothDevice, name: String, rssi: Int) {}
        fun onConnectionStep(step: Int, desc: String) {}
        fun onConnecting(deviceName: String)
        fun onLineReceived(line: String)
        fun onScreenFrame(hex: String)
        fun onCapturedFixed(code: Long, codeHex: String, bitLen: Int, pulseLen: Int, proto: Int, freq: Double, rssi: Int)
        fun onCapturedRaw(proto: String, isKeeloq: Boolean, serial: Long, btn: Int, enc: Long, count: Int, freq: Double, rssi: Int, pulses: List<Int>)
        fun onDeviceReady(device: String, ver: String)
        fun onPong(device: String, ver: String, chip: String, cc1101: Boolean, heap: Long, sigCount: Int, maxSig: Int, freq: Double, pa: Int, squelch: Int)
        fun onSignalsBegin(count: Int)
        fun onSignalItem(item: SignalItem)
        fun onSignalsEnd()
        fun onTxDone(ok: Boolean, msg: String)
        fun onSaveDone(ok: Boolean, count: Int)
        fun onDeleteDone(ok: Boolean, count: Int)
        fun onClearDone(ok: Boolean)
        fun onConfigReceived(freqIdx: Int, freqVal: Double, paIdx: Int, paVal: Int, squelchIdx: Int, squelchVal: Int)
        fun onCarrierDone(status: String)
        fun onSnifferStarted(mode: String)
        fun onSnifferStopped()
        fun onLog(msg: String, isError: Boolean = false)
    }

    var listener: Listener? = null
    var lastFoundDevice: BluetoothDevice? = null
    var autoConnectOnFound: Boolean = true

    private val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val adapter: BluetoothAdapter? = bluetoothManager.adapter
    private var gatt: BluetoothGatt? = null
    private var rxChar: BluetoothGattCharacteristic? = null
    private var isConnected = false
    private var isScanning = false
    private var isConnecting = false
    private var targetDevice: BluetoothDevice? = null
    private var connectRetryCount = 0
    private var mtuNegotiated = false
    private var channelReadyReported = false

    private val mainHandler = Handler(Looper.getMainLooper())
    private val rxStringBuilder = StringBuilder()
    private var lastFrameTime = 0L

    private val watchdogRunnable = object : Runnable {
        override fun run() {
            if (isConnected) {
                if (System.currentTimeMillis() - lastFrameTime > 3000) {
                    sendCommand("STREAM_SCREEN 1")
                    sendCommand("GET_SCREEN")
                }
                mainHandler.postDelayed(this, 2000)
            }
        }
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult?) {
            if (isConnecting || isConnected) return
            val device = result?.device ?: return
            val advName = result.scanRecord?.deviceName ?: device.name ?: ""
            val isGhostRf = advName.contains("GHOSTRF", ignoreCase = true) ||
                            advName.contains("RF-HUB", ignoreCase = true) ||
                            advName.contains("ESP32", ignoreCase = true)
            val hasUartUuid = result.scanRecord?.serviceUuids?.any { it.uuid == SERVICE_UUID } == true

            if (isGhostRf || hasUartUuid) {
                val displayName = if (advName.isNotEmpty()) advName else "GHOSTRF"
                lastFoundDevice = device
                targetDevice = device
                val rssi = result.rssi
                mainHandler.post {
                    listener?.onDeviceFound(device, displayName, rssi)
                    listener?.onConnectionStep(1, "BLE маяк обнаружен: $displayName ($rssi dBm)")
                    listener?.onLog("[BLE] Найдено: $displayName [${device.address}], RSSI: $rssi dBm")
                }
                if (autoConnectOnFound) {
                    isConnecting = true
                    stopScan()
                    mainHandler.post {
                        listener?.onConnecting(displayName)
                    }
                    mainHandler.postDelayed({
                        connect(device)
                    }, 100)
                }
            }
        }

        override fun onScanFailed(errorCode: Int) {
            isScanning = false
            mainHandler.post {
                listener?.onScanStateChange(false)
                listener?.onLog("[BLE ERR] Ошибка сканирования: $errorCode", true)
            }
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                isConnected = false
                rxChar = null
                stopWatchdog()
                disconnectGattOnly()

                if (connectRetryCount < 3 && targetDevice != null) {
                    connectRetryCount++
                    mainHandler.post {
                        listener?.onLog("[BLE] Сбой GATT (статус $status). Повтор $connectRetryCount/3...")
                        listener?.onConnectionStep(1, "Повтор подключения ($connectRetryCount/3)...")
                    }
                    mainHandler.postDelayed({
                        targetDevice?.let { connect(it) }
                    }, 250)
                    return
                }

                connectRetryCount = 0
                mainHandler.post {
                    listener?.onConnectionStateChange(false, "")
                    listener?.onLog("[BLE ERR] Сбой соединения GATT (status=$status).", true)
                }
                return
            }

            if (newState == BluetoothProfile.STATE_CONNECTED) {
                isConnected = true
                val name = gatt?.device?.name ?: "GHOSTRF"
                mainHandler.post {
                    listener?.onConnectionStateChange(true, name)
                    listener?.onConnectionStep(2, "Соединение GATT установлено. Приоритет скорости...")
                    listener?.onLog("[BLE] Соединение установлено с $name, перевод в High Priority...")
                }

                try {
                    gatt?.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)
                } catch (_: Exception) {}

                mtuNegotiated = false
                mainHandler.postDelayed({
                    if (!mtuNegotiated && isConnected && this@BleManager.gatt != null) {
                        listener?.onLog("[BLE] MTU таймаут, переход к поиску сервисов...")
                        this@BleManager.gatt?.discoverServices()
                    }
                }, 400)

                mainHandler.postDelayed({
                    try {
                        this@BleManager.gatt?.requestMtu(512)
                    } catch (_: Exception) {
                        this@BleManager.gatt?.discoverServices()
                    }
                }, 80)
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                isConnected = false
                rxChar = null
                stopWatchdog()
                disconnectGattOnly()
                mainHandler.post {
                    listener?.onConnectionStateChange(false, "")
                    listener?.onLog("[BLE] Отключено от устройства.")
                }
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt?, mtu: Int, status: Int) {
            mtuNegotiated = true
            mainHandler.post {
                listener?.onConnectionStep(2, "MTU $mtu байт согласован. Поиск сервисов...")
                listener?.onLog("[BLE] MTU установлен: $mtu, поиск сервисов...")
            }
            mainHandler.postDelayed({
                this@BleManager.gatt?.discoverServices()
            }, 50)
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val service = gatt?.getService(SERVICE_UUID)
                if (service != null) {
                    rxChar = service.getCharacteristic(RX_CHAR_UUID)
                    val txChar = service.getCharacteristic(TX_CHAR_UUID)
                    if (txChar != null) {
                        gatt.setCharacteristicNotification(txChar, true)
                        val descriptor = txChar.getDescriptor(CCCD_UUID)
                        if (descriptor != null) {
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                                gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
                            } else {
                                @Suppress("DEPRECATION")
                                descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                                @Suppress("DEPRECATION")
                                gatt.writeDescriptor(descriptor)
                            }
                            mainHandler.postDelayed({
                                if (!channelReadyReported) {
                                    onChannelReady()
                                }
                            }, 450)
                            return
                        }
                    }
                    onChannelReady()
                } else {
                    mainHandler.post {
                        listener?.onLog("[BLE ERR] Сервис Nordic UART не найден!", true)
                    }
                }
            } else {
                mainHandler.post {
                    listener?.onLog("[BLE ERR] Ошибка поиска сервисов: $status", true)
                }
            }
        }

        override fun onDescriptorWrite(gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                onChannelReady()
            } else {
                mainHandler.post {
                    listener?.onLog("[BLE ERR] Ошибка подписки на нотификации: $status", true)
                }
                onChannelReady()
            }
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?) {
            @Suppress("DEPRECATION")
            val bytes = characteristic?.value ?: return
            handleReceivedBytes(bytes)
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            handleReceivedBytes(value)
        }
    }

    private fun handleReceivedBytes(bytes: ByteArray) {
        val chunk = String(bytes, Charsets.UTF_8)
        synchronized(rxStringBuilder) {
            rxStringBuilder.append(chunk)
            var newlineIdx = rxStringBuilder.indexOf("\n")
            while (newlineIdx != -1) {
                val line = rxStringBuilder.substring(0, newlineIdx).trim()
                rxStringBuilder.delete(0, newlineIdx + 1)
                if (line.isNotEmpty()) {
                    mainHandler.post { parseIncomingLine(line) }
                }
                newlineIdx = rxStringBuilder.indexOf("\n")
            }
        }
    }

    private fun onChannelReady() {
        if (channelReadyReported) return
        channelReadyReported = true
        connectRetryCount = 0
        mainHandler.post {
            listener?.onConnectionStep(3, "Канал связи готов. Синхронизация с ESP32...")
            listener?.onLog("[BLE OK] Канал связи готов!")
            sendCommand("PING")
            sendCommand("GET_CONFIG")
            sendCommand("GET_SIGNALS")
            sendCommand("STREAM_SCREEN 1")
            mainHandler.postDelayed({ sendCommand("GET_SCREEN") }, 80)
            startWatchdog()
        }
    }

    fun startScan() {
        if (adapter == null || !adapter.isEnabled) {
            listener?.onLog("[BLE ERR] Bluetooth выключен!", true)
            return
        }
        if (isScanning) return
        isScanning = true
        listener?.onScanStateChange(true)
        listener?.onLog("[BLE] Поиск GHOSTRF...")

        val scanner = adapter.bluetoothLeScanner
        if (scanner == null) {
            listener?.onLog("[BLE ERR] BLE сканер недоступен", true)
            isScanning = false
            listener?.onScanStateChange(false)
            return
        }

        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        scanner.startScan(null, settings, scanCallback)

        mainHandler.postDelayed({
            if (isScanning) {
                stopScan()
                listener?.onLog("[BLE] Устройство не обнаружено (таймаут). Попробуйте еще раз.")
            }
        }, 8000)
    }

    fun stopScan() {
        if (isScanning) {
            isScanning = false
            adapter?.bluetoothLeScanner?.stopScan(scanCallback)
            mainHandler.post { listener?.onScanStateChange(false) }
        }
    }

    fun isScanning(): Boolean = isScanning

    fun connect(device: BluetoothDevice) {
        targetDevice = device
        isConnecting = true
        disconnectGattOnly()
        mainHandler.postDelayed({
            try {
                gatt = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
                } else {
                    device.connectGatt(context, false, gattCallback)
                }
            } catch (e: Exception) {
                isConnecting = false
                listener?.onLog("[BLE ERR] Ошибка connectGatt: ${e.message}", true)
            }
        }, 80)
    }

    private fun startWatchdog() {
        stopWatchdog()
        lastFrameTime = System.currentTimeMillis()
        mainHandler.postDelayed(watchdogRunnable, 2000)
    }

    private fun stopWatchdog() {
        mainHandler.removeCallbacks(watchdogRunnable)
    }

    private fun disconnectGattOnly() {
        try {
            gatt?.disconnect()
            gatt?.close()
        } catch (_: Exception) {}
        gatt = null
        isConnected = false
        isConnecting = false
        rxChar = null
        mtuNegotiated = false
        channelReadyReported = false
    }

    fun disconnect() {
        targetDevice = null
        connectRetryCount = 0
        stopWatchdog()
        disconnectGattOnly()
    }

    fun isConnected(): Boolean = isConnected

    @Synchronized
    fun sendCommand(cmd: String): Boolean {
        val g = gatt ?: return false
        val r = rxChar ?: return false
        val data = (cmd.trim() + "\n").toByteArray(Charsets.UTF_8)
        return try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                val res = g.writeCharacteristic(r, data, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE)
                res == BluetoothStatusCodes.SUCCESS
            } else {
                @Suppress("DEPRECATION")
                r.value = data
                @Suppress("DEPRECATION")
                r.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
                @Suppress("DEPRECATION")
                g.writeCharacteristic(r)
            }
        } catch (_: Exception) {
            false
        }
    }

    // Convenience command API
    fun requestPong() = sendCommand("PING")
    fun requestConfig() = sendCommand("GET_CONFIG")
    fun setConfig(freqIdx: Int, paIdx: Int, squelchIdx: Int) = sendCommand("SET_CONFIG $freqIdx $paIdx $squelchIdx")
    fun requestSignals() = sendCommand("GET_SIGNALS")
    fun transmitSaved(idx: Int) = sendCommand("TX_SAVED $idx")
    fun transmitFixed(freq: Double, code: Long, bitLen: Int, pulseLen: Int, proto: Int, pa: Int, repeats: Int) =
        sendCommand(String.format(Locale.US, "TX_FIXED %.3f %d %d %d %d %d %d", freq, code, bitLen, pulseLen, proto, pa, repeats))
    fun transmitRaw(freq: Double, pa: Int, repeats: Int, pulses: List<Int>) {
        val sb = StringBuilder(String.format(Locale.US, "TX_RAW %.3f %d %d %d", freq, pa, repeats, pulses.size))
        for (p in pulses) sb.append(" ").append(p)
        sendCommand(sb.toString())
    }
    fun saveFixed(name: String, freq: Double, code: Long, bitLen: Int, pulseLen: Int, proto: Int, repeats: Int, protoName: String) =
        sendCommand(String.format(Locale.US, "SAVE_FIXED %s %.3f %d %d %d %d %d %s", name.replace(' ', '_'), freq, code, bitLen, pulseLen, proto, repeats, protoName))
    fun saveRaw(name: String, freq: Double, repeats: Int, protoName: String, isKeeloq: Boolean, serial: Long, btn: Int, enc: Long, pulses: List<Int>) {
        val sb = StringBuilder(String.format(Locale.US, "SAVE_RAW %s %.3f %d %s %d %d %d %d %d",
            name.replace(' ', '_'), freq, repeats, protoName, if (isKeeloq) 1 else 0, serial, btn, enc, pulses.size))
        for (p in pulses) sb.append(" ").append(p)
        sendCommand(sb.toString())
    }
    fun deleteSignal(idx: Int) = sendCommand("DELETE_SIGNAL $idx")
    fun clearSignals() = sendCommand("CLEAR_SIGNALS")
    fun sendRemoteBtn(btn: String) = sendCommand("REMOTE_BTN $btn")
    fun setDeviceMode(mode: String) = sendCommand("GOTO_MODE $mode")
    fun startSniffer(mode: String) = sendCommand("START_SNIFFER $mode")
    fun stopSniffer() = sendCommand("STOP_SNIFFER")
    fun setCarrier(mode: Int, freq: Double, pa: Int, on: Boolean) =
        sendCommand(String.format(Locale.US, "SET_CARRIER %d %.3f %d %d", mode, freq, pa, if (on) 1 else 0))
    fun rebootDevice() = sendCommand("REBOOT")

    private fun parseIncomingLine(line: String) {
        listener?.onLineReceived(line)
        if (!line.startsWith("{")) return

        try {
            val json = JSONObject(line)
            if (json.has("cmd")) {
                when (json.getString("cmd")) {
                    "SCREEN" -> {
                        if (json.has("hex")) {
                            lastFrameTime = System.currentTimeMillis()
                            listener?.onScreenFrame(json.getString("hex"))
                        }
                    }
                    "PONG" -> {
                        val dev = json.optString("device", "GHOSTRF")
                        val ver = json.optString("ver", "1.2.0")
                        val chip = json.optString("chip", "ESP32-C3")
                        val cc = json.optBoolean("cc1101", true)
                        val heap = json.optLong("free_heap", 0L)
                        val sc = json.optInt("signals_count", 0)
                        val maxS = json.optInt("max_signals", 20)
                        val f = json.optDouble("freq", 433.92)
                        val pa = json.optInt("pa", 10)
                        val sq = json.optInt("squelch", -72)
                        listener?.onConnectionStep(3, "Синхронизация завершена. Готово к работе!")
                        listener?.onPong(dev, ver, chip, cc, heap, sc, maxS, f, pa, sq)
                    }
                    "SIGNALS_BEGIN" -> {
                        val count = json.optInt("count", 0)
                        listener?.onSignalsBegin(count)
                    }
                    "SIGNAL" -> {
                        val item = SignalItem.fromJson(json)
                        listener?.onSignalItem(item)
                    }
                    "SIGNALS_END" -> {
                        listener?.onSignalsEnd()
                    }
                    "TX_DONE" -> {
                        val ok = json.optString("status") == "OK"
                        val msg = json.optString("msg", "")
                        listener?.onTxDone(ok, msg)
                    }
                    "SAVE_DONE" -> {
                        val ok = json.optString("status") == "OK"
                        val count = json.optInt("count", 0)
                        listener?.onSaveDone(ok, count)
                    }
                    "DELETE_DONE" -> {
                        val ok = json.optString("status") == "OK"
                        val count = json.optInt("count", 0)
                        listener?.onDeleteDone(ok, count)
                    }
                    "CLEAR_DONE" -> {
                        val ok = json.optString("status") == "OK"
                        listener?.onClearDone(ok)
                    }
                    "CONFIG" -> {
                        val fIdx = json.optInt("freq_idx", 0)
                        val fVal = json.optDouble("freq_val", 433.92)
                        val pIdx = json.optInt("pa_idx", 3)
                        val pVal = json.optInt("pa_val", 10)
                        val sqIdx = json.optInt("squelch_idx", 1)
                        val sqVal = json.optInt("squelch_val", -78)
                        listener?.onConfigReceived(fIdx, fVal, pIdx, pVal, sqIdx, sqVal)
                    }
                    "SNIFFER_STARTED" -> {
                        val mode = json.optString("mode", "FIXED")
                        listener?.onSnifferStarted(mode)
                    }
                    "SNIFFER_STOPPED" -> {
                        listener?.onSnifferStopped()
                    }
                    "CARRIER_DONE" -> {
                        val status = json.optString("status", "OFF")
                        listener?.onCarrierDone(status)
                    }
                }
            } else if (json.has("event")) {
                when (json.getString("event")) {
                    "DEVICE_READY" -> {
                        sendCommand("STREAM_SCREEN 1")
                        mainHandler.postDelayed({ sendCommand("GET_SCREEN") }, 80)
                        mainHandler.postDelayed({ sendCommand("PING") }, 150)
                        mainHandler.postDelayed({ sendCommand("GET_CONFIG") }, 250)
                        mainHandler.postDelayed({ sendCommand("GET_SIGNALS") }, 350)
                        listener?.onDeviceReady(json.optString("device", "GHOSTRF"), json.optString("ver", "1.2.0"))
                    }
                    "CAPTURED_FIXED" -> {
                        val code = json.optLong("code", 0)
                        val codeHex = json.optString("codeHex", "0x0")
                        val bitLen = json.optInt("bitLen", 24)
                        val pulseLen = json.optInt("pulseLen", 320)
                        val proto = json.optInt("proto", 1)
                        val freq = json.optDouble("freq", 433.92)
                        val rssi = json.optInt("rssi", -70)
                        listener?.onCapturedFixed(code, codeHex, bitLen, pulseLen, proto, freq, rssi)
                    }
                    "CAPTURED_RAW" -> {
                        val proto = json.optString("proto", "RAW")
                        val isKeeloq = json.optBoolean("isKeeloq", false)
                        val serial = json.optLong("serial", 0)
                        val btn = json.optInt("btn", 0)
                        val enc = json.optLong("enc", 0)
                        val count = json.optInt("count", 0)
                        val freq = json.optDouble("freq", 433.92)
                        val rssi = json.optInt("rssi", -70)
                        val pulsesList = mutableListOf<Int>()
                        val pArray = json.optJSONArray("pulses")
                        if (pArray != null) {
                            for (i in 0 until pArray.length()) {
                                pulsesList.add(pArray.getInt(i))
                            }
                        }
                        listener?.onCapturedRaw(proto, isKeeloq, serial, btn, enc, count, freq, rssi, pulsesList)
                    }
                }
            }
        } catch (_: Exception) {}
    }
}
