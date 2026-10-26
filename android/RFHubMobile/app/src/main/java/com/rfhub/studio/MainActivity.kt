package com.rfhub.studio

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.rfhub.studio.databinding.ActivityMainBinding
import com.rfhub.studio.databinding.DialogEditSignalBinding
import com.rfhub.studio.databinding.ItemSignalCardBinding
import com.rfhub.studio.databinding.ItemSnifferPacketBinding
import org.json.JSONArray
import java.text.SimpleDateFormat
import java.util.*
import kotlin.math.abs

class MainActivity : AppCompatActivity(), BleManager.Listener {

    private lateinit var binding: ActivityMainBinding
    private lateinit var bleManager: BleManager
    private val timeFormat = SimpleDateFormat("HH:mm:ss", Locale.getDefault())

    // Freq & Power arrays
    private val freqs = arrayOf("433.92 МГц", "433.07 МГц", "433.42 МГц", "434.42 МГц", "434.77 МГц", "315.00 МГц", "868.35 МГц")
    private val freqValues = doubleArrayOf(433.92, 433.07, 433.42, 434.42, 434.77, 315.00, 868.35)
    private val powers = arrayOf("-30 dBm", "-10 dBm", "0 dBm", "+10 dBm")
    private val paLevels = intArrayOf(-30, -10, 0, 10)
    private val squelches = arrayOf("-85 dBm (MAX)", "-78 dBm (MID)", "-72 dBm (BASE)", "-65 dBm (LOW)")
    private val squelchLevels = intArrayOf(-85, -78, -72, -65)
    private val repeatOptions = arrayOf("1", "3", "5", "7", "10", "15", "20")
    private val fixedProtocols = arrayOf("EV1527", "PT2262", "Came 12bit", "Came 24bit", "Nice 12bit", "DoorHan", "Linear 10bit", "RC-Switch 1")

    // State
    private var isStreaming = true
    private var isInverted = false
    private var isCarrierOn = false
    private var currentTab = 0 // 0..4

    // Data lists
    private val savedSignals = mutableListOf<SignalItem>()
    private var selectedSignalIdx = -1

    data class SnifferEntry(val timeStr: String, val item: SignalItem, val rssi: Int)
    private val snifferHistory = mutableListOf<SnifferEntry>()
    private var selectedSnifferIdx = -1

    private var updatingHexDec = false
    private var isOfflineMode = false

    companion object {
        private const val GHOSTRF_SPLASH_HEX = "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000008080c0c0c0c0c0c0c0c0c0c08080800000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000080f0feffffffff0f0f0fffffffffffff0f0f0ffffefcf8f0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000fffffffffffffffffcfcfc8f07078ffffffcfcfcffffffffffc0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000f8fcffffffffffffffffffffffffffffffffffffffffffffffffffff7f7e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000030307070f1f1f0f0f0f0f1f3f3f3f3f1f0f0f0f1f1f1f0f070300000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000c0c0303030303030f0f00000f0f0000000000000f0f00000c0c0303030303030c0c00000c0c0303030303030c0c00000f0f03030f0f03030f0f00000f0f0303030303030c0c00000f0f0303030303030303000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000ffff000000003030f0f00000ffff0c0c0c0c0c0cffff0000ffff000000000000ffff0000c3c30c0c0c0c0c0cf0f0000000000000ffff000000000000ffff0c0c3c3ccccc03030000ffff0c0c0c0c0c0c00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000303030303030303000003030000000000000303000000000303030303030000000000000303030303030000000000000000030300000000000003030000000000000303000003030000000000000000000000000000000000000000000000000000000000000000"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        setTheme(R.style.Theme_RFHubStudio)
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Preload OLED preview canvases with device splash frame (Pixel Ghost & GHOSTRF)
        binding.chassisOledCanvas.updateFrameFromHex(GHOSTRF_SPLASH_HEX)
        binding.oledView.updateFrameFromHex(GHOSTRF_SPLASH_HEX)

        bleManager = BleManager(this)
        bleManager.listener = this

        setupConnectScreen()
        setupTabNavigation()
        setupTopBar()
        setupDisplayTab()
        setupSignalsTab()
        setupTransmitterTab()
        setupSnifferTab()
        setupConsoleTab()

        checkPermissionsAndStart()
    }

    private fun showConnectScreen(statusMsg: String? = null) {
        binding.layoutConnectScreen.visibility = View.VISIBLE
        binding.layoutMainStudio.visibility = View.GONE
        if (statusMsg != null) {
            binding.tvScanStatus.text = statusMsg
        }
    }

    private fun showMainStudio() {
        binding.layoutConnectScreen.visibility = View.GONE
        binding.layoutMainStudio.visibility = View.VISIBLE
    }

    private fun setupConnectScreen() {
        binding.btnConnectFound.setOnClickListener {
            val dev = bleManager.lastFoundDevice
            if (dev != null) {
                binding.btnConnectFound.isEnabled = false
                binding.btnConnectFound.text = "[ СОЕДИНЕНИЕ... ]"
                binding.tvConnStep2.text = "[ ⟳ ] 2. Подключение GATT и согласование MTU 512..."
                binding.tvConnStep2.setTextColor(Color.WHITE)
                bleManager.connect(dev)
            } else {
                bleManager.startScan()
            }
        }

        binding.btnRetryScan.setOnClickListener {
            resetConnectionSteps()
            binding.cardFoundDevice.visibility = View.GONE
            binding.tvScanStatus.text = "[ ⟳ ПОИСК GHOSTRF В ЭФИРЕ BLE... ]"
            binding.pbScan.visibility = View.VISIBLE
            bleManager.startScan()
        }

        binding.btnOfflineMode.setOnClickListener {
            isOfflineMode = true
            binding.tvStatus.text = "[ АВТОНОМНЫЙ РЕЖИМ ]"
            binding.tvStatus.setTextColor(Color.parseColor("#FFAA00"))
            binding.btnConnect.text = "[ ПОДКЛЮЧИТЬ ]"
            binding.tvTelemetry.text = "[ АВТОНОМНО: БЕЗ ПОДКЛЮЧЕНИЯ К ПЛАТЕ ]"
            showMainStudio()
        }
    }

    private fun resetConnectionSteps() {
        binding.tvConnStep1.text = "[ ○ ] 1. BLE радиомаяк GHOSTRF"
        binding.tvConnStep1.setTextColor(ContextCompat.getColor(this, R.color.text_secondary))
        binding.tvConnStep2.text = "[ ○ ] 2. Подключение GATT и согласование MTU 512"
        binding.tvConnStep2.setTextColor(ContextCompat.getColor(this, R.color.text_secondary))
        binding.tvConnStep3.text = "[ ○ ] 3. Синхронизация сигналов NVS и стриминга экрана"
        binding.tvConnStep3.setTextColor(ContextCompat.getColor(this, R.color.text_secondary))
    }

    // --- NAVIGATION TABS ---
    private fun setupTabNavigation() {
        binding.btnTabDisplay.setOnClickListener { switchTab(0) }
        binding.btnTabSignals.setOnClickListener { switchTab(1) }
        binding.btnTabTransmitter.setOnClickListener { switchTab(2) }
        binding.btnTabSniffer.setOnClickListener { switchTab(3) }
        binding.btnTabConsole.setOnClickListener { switchTab(4) }
        switchTab(0)
    }

    private fun switchTab(tabIndex: Int) {
        currentTab = tabIndex
        val tabs = arrayOf(
            binding.btnTabDisplay,
            binding.btnTabSignals,
            binding.btnTabTransmitter,
            binding.btnTabSniffer,
            binding.btnTabConsole
        )
        val containers = arrayOf(
            binding.tabDisplayContainer,
            binding.tabSignalsContainer,
            binding.tabTransmitterContainer,
            binding.tabSnifferContainer,
            binding.tabConsoleContainer
        )

        for (i in tabs.indices) {
            val isActive = (i == tabIndex)
            tabs[i].setBackgroundResource(if (isActive) R.drawable.square_btn_tab_active else R.drawable.square_btn_tab_inactive)
            tabs[i].setTextColor(if (isActive) Color.BLACK else Color.WHITE)
            containers[i].visibility = if (isActive) View.VISIBLE else View.GONE
        }
    }

    // --- TOP BAR ---
    private fun setupTopBar() {
        binding.btnConnect.setOnClickListener {
            if (bleManager.isConnected()) {
                bleManager.disconnect()
                showConnectScreen("[ ОТКЛЮЧЕНО ПОЛЬЗОВАТЕЛЕМ ]")
                resetConnectionSteps()
                binding.cardFoundDevice.visibility = View.GONE
                bleManager.startScan()
            } else {
                showConnectScreen("[ ПОИСК GHOSTRF... ]")
                resetConnectionSteps()
                binding.cardFoundDevice.visibility = View.GONE
                bleManager.startScan()
            }
        }
    }

    // --- TAB 1: DISPLAY & REMOTE ---
    private fun setupDisplayTab() {
        binding.btnToggleStream.setOnClickListener {
            isStreaming = !isStreaming
            binding.btnToggleStream.text = if (isStreaming) "[ СТРИМ: ВКЛ ]" else "[ СТРИМ: ВЫКЛ ]"
            bleManager.sendCommand(if (isStreaming) "STREAM_SCREEN 1" else "STREAM_SCREEN 0")
        }

        binding.btnInvert.setOnClickListener {
            isInverted = !isInverted
            binding.oledView.setInverted(isInverted)
        }

        binding.btnGrid.text = if (binding.oledView.hasPixelGrid) "[ СЕТКА: ВКЛ ]" else "[ СЕТКА: ВЫКЛ ]"
        binding.btnGrid.setOnClickListener {
            val isGrid = binding.oledView.toggleGrid()
            binding.btnGrid.text = if (isGrid) "[ СЕТКА: ВКЛ ]" else "[ СЕТКА: ВЫКЛ ]"
            appendConsole(if (isGrid) "[ДИСПЛЕЙ] Сетка пикселей включена" else "[ДИСПЛЕЙ] Монолитная картинка включена")
        }

        // D-pad
        binding.btnUp.setOnClickListener { bleManager.sendRemoteBtn("UP") }
        binding.btnOk.setOnClickListener { bleManager.sendRemoteBtn("OK") }
        binding.btnDown.setOnClickListener { bleManager.sendRemoteBtn("DOWN") }
        binding.btnHoldMenu.setOnClickListener { bleManager.sendRemoteBtn("OK_HOLD") }

        // Quick modes
        binding.btnModeMain.setOnClickListener { bleManager.setDeviceMode("MAIN") }
        binding.btnModeRx.setOnClickListener { bleManager.setDeviceMode("RX") }
        binding.btnModeRaw.setOnClickListener { bleManager.setDeviceMode("RAW") }
        binding.btnModeSpectrum.setOnClickListener { bleManager.setDeviceMode("SPECTRUM") }
        binding.btnModeFreq.setOnClickListener { bleManager.setDeviceMode("FREQ") }
        binding.btnModeTx.setOnClickListener { bleManager.setDeviceMode("TX") }
        binding.btnModeCarrier.setOnClickListener { bleManager.setDeviceMode("CARRIER") }
        binding.btnMode24G.setOnClickListener { bleManager.setDeviceMode("24G") }
        binding.btnModeBattery.setOnClickListener { bleManager.setDeviceMode("BATTERY") }
        binding.btnModeSettings.setOnClickListener { bleManager.setDeviceMode("SETTINGS") }
        binding.btnModeEditor.setOnClickListener { bleManager.setDeviceMode("NEW_SIG") }
        binding.btnReboot.setOnClickListener { bleManager.rebootDevice() }

        // Radio spinners
        val adFreq = ArrayAdapter(this, R.layout.spinner_item, freqs).apply { setDropDownViewResource(R.layout.spinner_dropdown_item) }
        val adPower = ArrayAdapter(this, R.layout.spinner_item, powers).apply { setDropDownViewResource(R.layout.spinner_dropdown_item) }
        val adSquelch = ArrayAdapter(this, R.layout.spinner_item, squelches).apply { setDropDownViewResource(R.layout.spinner_dropdown_item) }

        binding.spFreq.adapter = adFreq
        binding.spPower.adapter = adPower
        binding.spSquelch.adapter = adSquelch
        binding.spPower.setSelection(3) // +10 dBm
        binding.spSquelch.setSelection(1) // -78 dBm

        binding.btnApplyRadio.setOnClickListener {
            val f = binding.spFreq.selectedItemPosition
            val p = binding.spPower.selectedItemPosition
            val sq = binding.spSquelch.selectedItemPosition
            bleManager.setConfig(f, p, sq)
            appendConsole("[CFG] Отправлены параметры радио в NVS")
        }

        binding.btnRebootEsp.setOnClickListener {
            AlertDialog.Builder(this)
                .setTitle("Перезагрузка")
                .setMessage("Перезагрузить устройство ESP32-C3?")
                .setPositiveButton("Да") { _, _ -> bleManager.rebootDevice() }
                .setNegativeButton("Отмена", null)
                .show()
        }
    }

    // --- TAB 2: SAVED SIGNALS ---
    private fun setupSignalsTab() {
        val repAdapter = ArrayAdapter(this, R.layout.spinner_item, repeatOptions).apply { setDropDownViewResource(R.layout.spinner_dropdown_item) }
        binding.spSavedRepeats.adapter = repAdapter
        binding.spSavedRepeats.setSelection(2) // 5 repeats

        binding.btnReloadSignals.setOnClickListener {
            savedSignals.clear()
            renderSignalsList()
            bleManager.requestSignals()
            appendConsole("[NVS] Запрос списка сохраненных сигналов...")
        }

        binding.btnAddSignalDialog.setOnClickListener {
            openSignalEditDialog(SignalItem(), isNew = true)
        }

        binding.btnDeleteSignal.setOnClickListener {
            if (selectedSignalIdx in savedSignals.indices) {
                val item = savedSignals[selectedSignalIdx]
                AlertDialog.Builder(this)
                    .setTitle("Удаление сигнала")
                    .setMessage("Удалить сигнал '${item.name}' из памяти NVS?")
                    .setPositiveButton("Удалить") { _, _ ->
                        bleManager.deleteSignal(selectedSignalIdx)
                    }
                    .setNegativeButton("Отмена", null)
                    .show()
            } else {
                Toast.makeText(this, "Выберите сигнал из списка", Toast.LENGTH_SHORT).show()
            }
        }

        binding.btnClearSignals.setOnClickListener {
            AlertDialog.Builder(this)
                .setTitle("Очистка NVS")
                .setMessage("ВНИМАНИЕ: Очистить ВСЕ сохраненные сигналы на устройстве?")
                .setPositiveButton("Очистить все") { _, _ ->
                    bleManager.clearSignals()
                }
                .setNegativeButton("Отмена", null)
                .show()
        }

        binding.btnTransmitSaved.setOnClickListener {
            if (selectedSignalIdx in savedSignals.indices) {
                val reps = repeatOptions[binding.spSavedRepeats.selectedItemPosition].toInt()
                val item = savedSignals[selectedSignalIdx]
                item.repeats = reps
                bleManager.transmitSaved(selectedSignalIdx)
                appendConsole("[TX] Передача сигнала #${selectedSignalIdx + 1} (${item.name})...")
            }
        }

        binding.btnSavedWaveZoomIn.setOnClickListener { binding.waveformSaved.zoomIn() }
        binding.btnSavedWaveZoomOut.setOnClickListener { binding.waveformSaved.zoomOut() }
        binding.btnSavedWaveReset.setOnClickListener { binding.waveformSaved.resetZoom() }
        binding.btnSavedWaveInvert.setOnClickListener { binding.waveformSaved.toggleInvert() }

        // JSON Backup export
        binding.btnExportJson.setOnClickListener {
            if (savedSignals.isEmpty()) {
                Toast.makeText(this, "Список сигналов пуст", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            val arr = JSONArray()
            for (s in savedSignals) arr.put(s.toJson())
            val jsonStr = arr.toString(2)
            val shareIntent = Intent(Intent.ACTION_SEND).apply {
                type = "application/json"
                putExtra(Intent.EXTRA_SUBJECT, "GHOSTRF_Signals_Backup.json")
                putExtra(Intent.EXTRA_TEXT, jsonStr)
            }
            startActivity(Intent.createChooser(shareIntent, "Экспорт бэкапа JSON"))
        }

        // Экспорт Sub-GHz .sub
        binding.btnExportSub.setOnClickListener {
            if (selectedSignalIdx in savedSignals.indices) {
                val item = savedSignals[selectedSignalIdx]
                val subContent = item.toSubGhzFile()
                val shareIntent = Intent(Intent.ACTION_SEND).apply {
                    type = "text/plain"
                    putExtra(Intent.EXTRA_SUBJECT, "${item.name}.sub")
                    putExtra(Intent.EXTRA_TEXT, subContent)
                }
                startActivity(Intent.createChooser(shareIntent, "Экспорт ключа Sub-GHz (.sub)"))
            } else {
                Toast.makeText(this, "Выберите сигнал для экспорта", Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun renderSignalsList() {
        binding.llSignalsList.removeAllViews()
        binding.tvSignalsCount.text = "СПИСОК СОХРАНЕННЫХ СИГНАЛОВ (${savedSignals.size} / 20)"

        for (i in savedSignals.indices) {
            val item = savedSignals[i]
            val cardBinding = ItemSignalCardBinding.inflate(LayoutInflater.from(this), binding.llSignalsList, false)
            cardBinding.tvCardIndexName.text = "#${i + 1}. ${item.name}"
            cardBinding.tvCardType.text = item.typeName()
            cardBinding.tvCardFreq.text = String.format(Locale.US, "%.3f МГц", item.freq)
            cardBinding.tvCardCode.text = if (item.type == SignalItem.SIG_KIND_RAW) {
                "${item.rawCount} имп."
            } else {
                "${item.codeHex()} (${item.bitLen}b)"
            }

            val isSelected = (i == selectedSignalIdx)
            cardBinding.root.setBackgroundResource(if (isSelected) R.drawable.square_btn_primary else R.drawable.square_btn_bg)
            if (isSelected) {
                cardBinding.tvCardIndexName.setTextColor(Color.BLACK)
                cardBinding.tvCardCode.setTextColor(Color.BLACK)
            }

            cardBinding.root.setOnClickListener {
                selectedSignalIdx = i
                renderSignalsList()
                showSelectedSignalInspector(item)
            }
            binding.llSignalsList.addView(cardBinding.root)
        }

        if (selectedSignalIdx in savedSignals.indices) {
            showSelectedSignalInspector(savedSignals[selectedSignalIdx])
        }
    }

    private fun showSelectedSignalInspector(item: SignalItem) {
        binding.tvInspectorTitle.text = "ИНСПЕКТОР: #${selectedSignalIdx + 1}. ${item.name} [${item.typeName()}]"
        binding.tvInspectorParams.text = String.format(Locale.US, "Частота: %.3f МГц | Код: %s (%d) | TE: %d мкс",
            item.freq, item.codeHex(), item.code, item.pulseLen)
        binding.tvInspectorBin.text = "BIN: " + item.codeBin()
        binding.waveformSaved.setPulses(item.toWaveformPulses())
    }

    // --- TAB 3: TRANSMITTER / GENERATOR ---
    private fun setupTransmitterTab() {
        val subFixed = binding.btnSubTabFixed
        val subRaw = binding.btnSubTabRaw
        val subCarrier = binding.btnSubTabCarrier

        fun switchTransmitterSubTab(idx: Int) {
            subFixed.setBackgroundResource(if (idx == 0) R.drawable.square_btn_tab_active else R.drawable.square_btn_tab_inactive)
            subFixed.setTextColor(if (idx == 0) Color.BLACK else Color.WHITE)
            subRaw.setBackgroundResource(if (idx == 1) R.drawable.square_btn_tab_active else R.drawable.square_btn_tab_inactive)
            subRaw.setTextColor(if (idx == 1) Color.BLACK else Color.WHITE)
            subCarrier.setBackgroundResource(if (idx == 2) R.drawable.square_btn_tab_active else R.drawable.square_btn_tab_inactive)
            subCarrier.setTextColor(if (idx == 2) Color.BLACK else Color.WHITE)

            binding.llGenFixedContainer.visibility = if (idx == 0) View.VISIBLE else View.GONE
            binding.llGenRawContainer.visibility = if (idx == 1) View.VISIBLE else View.GONE
            binding.llGenCarrierContainer.visibility = if (idx == 2) View.VISIBLE else View.GONE
        }

        subFixed.setOnClickListener { switchTransmitterSubTab(0) }
        subRaw.setOnClickListener { switchTransmitterSubTab(1) }
        subCarrier.setOnClickListener { switchTransmitterSubTab(2) }

        // Setup Fixed Form
        val adFreq = ArrayAdapter(this, R.layout.spinner_item, freqs).apply { setDropDownViewResource(R.layout.spinner_dropdown_item) }
        val adProto = ArrayAdapter(this, R.layout.spinner_item, fixedProtocols).apply { setDropDownViewResource(R.layout.spinner_dropdown_item) }
        val adReps = ArrayAdapter(this, R.layout.spinner_item, repeatOptions).apply { setDropDownViewResource(R.layout.spinner_dropdown_item) }

        binding.spFixedFreq.adapter = adFreq
        binding.spFixedProto.adapter = adProto
        binding.spFixedRepeats.adapter = adReps
        binding.spFixedRepeats.setSelection(3) // 7 reps

        binding.spFixedProto.onItemSelectedListener = object : android.widget.AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: android.widget.AdapterView<*>?, view: View?, position: Int, id: Long) {
                when (position) {
                    0 -> { binding.etFixedBitLen.setText("24"); binding.etFixedPulseLen.setText("320") }
                    1 -> { binding.etFixedBitLen.setText("24"); binding.etFixedPulseLen.setText("350") }
                    2 -> { binding.etFixedBitLen.setText("12"); binding.etFixedPulseLen.setText("320") }
                    3 -> { binding.etFixedBitLen.setText("24"); binding.etFixedPulseLen.setText("320") }
                    4 -> { binding.etFixedBitLen.setText("12"); binding.etFixedPulseLen.setText("700") }
                    5 -> { binding.etFixedBitLen.setText("24"); binding.etFixedPulseLen.setText("400") }
                    6 -> { binding.etFixedBitLen.setText("10"); binding.etFixedPulseLen.setText("1000") }
                    7 -> { binding.etFixedBitLen.setText("24"); binding.etFixedPulseLen.setText("350") }
                }
            }
            override fun onNothingSelected(parent: android.widget.AdapterView<*>?) {}
        }

        fun updateFixedWaveformPreview() {
            val codeStr = binding.etFixedHex.text.toString().trim()
            val code = codeStr.removePrefix("0x").removePrefix("0X").toLongOrNull(16) ?: 0L
            val bitLen = binding.etFixedBitLen.text.toString().toIntOrNull() ?: 24
            val te = binding.etFixedPulseLen.text.toString().toIntOrNull() ?: 320
            val proto = fixedProtocols[binding.spFixedProto.selectedItemPosition]

            val dummy = SignalItem(
                name = "Preview",
                code = code,
                bitLen = bitLen,
                pulseLen = te,
                protoName = proto
            )
            binding.waveformGenFixed.setPulses(dummy.toWaveformPulses())
        }

        binding.etFixedHex.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
            override fun afterTextChanged(s: Editable?) {
                if (updatingHexDec) return
                updatingHexDec = true
                val h = s.toString().trim().removePrefix("0x").removePrefix("0X")
                val v = h.toLongOrNull(16)
                if (v != null) {
                    binding.etFixedDec.setText(v.toString())
                }
                updatingHexDec = false
                updateFixedWaveformPreview()
            }
        })

        binding.etFixedDec.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
            override fun afterTextChanged(s: Editable?) {
                if (updatingHexDec) return
                updatingHexDec = true
                val d = s.toString().trim().toLongOrNull()
                if (d != null) {
                    binding.etFixedHex.setText("0x" + d.toString(16).uppercase())
                }
                updatingHexDec = false
                updateFixedWaveformPreview()
            }
        })

        binding.etFixedBitLen.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(s: Editable?) { updateFixedWaveformPreview() }
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
        })

        binding.etFixedPulseLen.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(s: Editable?) { updateFixedWaveformPreview() }
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
        })

        binding.btnTxFixed.setOnClickListener {
            val f = freqValues[binding.spFixedFreq.selectedItemPosition]
            val code = binding.etFixedHex.text.toString().removePrefix("0x").removePrefix("0X").toLongOrNull(16) ?: 0L
            val bitLen = binding.etFixedBitLen.text.toString().toIntOrNull() ?: 24
            val te = binding.etFixedPulseLen.text.toString().toIntOrNull() ?: 320
            val protoIdx = binding.spFixedProto.selectedItemPosition + 1
            val reps = repeatOptions[binding.spFixedRepeats.selectedItemPosition].toInt()
            bleManager.transmitFixed(f, code, bitLen, te, protoIdx, 10, reps)
            appendConsole("[TX] Передача фиксированного кода 0x${code.toString(16).uppercase()} ($f МГц)")
        }

        binding.btnSaveFixedToDevice.setOnClickListener {
            val name = binding.etFixedName.text.toString().trim().ifEmpty { "Fixed_Sig" }
            val f = freqValues[binding.spFixedFreq.selectedItemPosition]
            val code = binding.etFixedHex.text.toString().removePrefix("0x").removePrefix("0X").toLongOrNull(16) ?: 0L
            val bitLen = binding.etFixedBitLen.text.toString().toIntOrNull() ?: 24
            val te = binding.etFixedPulseLen.text.toString().toIntOrNull() ?: 320
            val protoIdx = binding.spFixedProto.selectedItemPosition + 1
            val protoName = fixedProtocols[binding.spFixedProto.selectedItemPosition]
            val reps = repeatOptions[binding.spFixedRepeats.selectedItemPosition].toInt()
            bleManager.saveFixed(name, f, code, bitLen, te, protoIdx, reps, protoName)
            appendConsole("[SAVE] Сохранение сигнала '$name' в NVS устройства...")
        }

        // Setup RAW Form
        fun parseRawPulses(): List<Int> {
            val txt = binding.etRawPulses.text.toString()
            val tokens = txt.split("[\\s,]+".toRegex()).filter { it.isNotEmpty() }
            val list = mutableListOf<Int>()
            for (t in tokens) {
                val v = t.toIntOrNull() ?: continue
                list.add(abs(v))
            }
            return list
        }

        binding.etRawPulses.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(s: Editable?) {
                binding.waveformGenRaw.setPulses(parseRawPulses())
            }
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
        })

        binding.btnRawPresetCame.setOnClickListener {
            binding.etRawName.setText("Came_12B")
            binding.etRawPulses.setText("320 11520 640 320 320 640 640 320 320 640 640 320 320 640 640 320 320 640 640 320 320 640 640 320 320 640")
        }

        binding.btnRawPresetNice.setOnClickListener {
            binding.etRawName.setText("Nice_Flo")
            binding.etRawPulses.setText("1400 25200 700 1400 1400 700 700 1400 1400 700 700 1400 1400 700 700 1400 1400 700 700 1400 1400 700 700 1400 1400 700")
        }

        binding.btnRawPresetDoorbell.setOnClickListener {
            binding.etRawName.setText("Doorbell_EV1527")
            binding.etRawPulses.setText("320 9920 960 320 320 960 960 320 320 960 960 320 320 960 960 320 320 960 960 320 320 960 960 320 320 960 960 320")
        }

        binding.btnTxRaw.setOnClickListener {
            val pulses = parseRawPulses()
            if (pulses.isEmpty()) {
                Toast.makeText(this, "Введите импульсы RAW", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            bleManager.transmitRaw(433.92, 10, 5, pulses)
            appendConsole("[TX] Передача RAW сигнала (${pulses.size} импульсов)")
        }

        binding.btnSaveRawToDevice.setOnClickListener {
            val pulses = parseRawPulses()
            if (pulses.isEmpty()) {
                Toast.makeText(this, "Введите импульсы RAW", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            val name = binding.etRawName.text.toString().trim().ifEmpty { "RAW_Sig" }
            bleManager.saveRaw(name, 433.92, 5, "RAW", false, 0, 0, 0, pulses)
            appendConsole("[SAVE] Сохранение RAW сигнала '$name' в NVS устройства...")
        }

        // Setup Carrier / Jammer
        val carrierModes = arrayOf("0: Немодулированная несущая (CW)", "1: Тональный сигнал 1 кГц", "2: Шумовой джеммер")
        val adCarrierMode = ArrayAdapter(this, R.layout.spinner_item, carrierModes).apply { setDropDownViewResource(R.layout.spinner_dropdown_item) }
        val adPower = ArrayAdapter(this, R.layout.spinner_item, powers).apply { setDropDownViewResource(R.layout.spinner_dropdown_item) }
        binding.spCarrierMode.adapter = adCarrierMode
        binding.spCarrierFreq.adapter = adFreq
        binding.spCarrierPower.adapter = adPower
        binding.spCarrierPower.setSelection(3) // +10 dBm

        binding.btnToggleCarrier.setOnClickListener {
            isCarrierOn = !isCarrierOn
            val m = binding.spCarrierMode.selectedItemPosition
            val f = freqValues[binding.spCarrierFreq.selectedItemPosition]
            val p = paLevels[binding.spCarrierPower.selectedItemPosition]
            bleManager.setCarrier(m, f, p, isCarrierOn)
            binding.btnToggleCarrier.text = if (isCarrierOn) "[ ВЫКЛЮЧИТЬ ПЕРЕДАЧУ НЕСУЩЕЙ ]" else "[ ВКЛЮЧИТЬ ПЕРЕДАЧУ НЕСУЩЕЙ ]"
            appendConsole(if (isCarrierOn) "[RF] Несущая/Джеммер ВКЛЮЧЕН ($f МГц)" else "[RF] Несущая/Джеммер ВЫКЛЮЧЕН")
        }

        updateFixedWaveformPreview()
    }

    // --- TAB 4: SNIFFER ---
    private fun setupSnifferTab() {
        binding.btnStartSnifferFixed.setOnClickListener {
            bleManager.startSniffer("FIXED")
            appendConsole("[SNIFFER] Запущен приемник фиксированных кодов")
        }

        binding.btnStartSnifferRaw.setOnClickListener {
            bleManager.startSniffer("RAW")
            appendConsole("[SNIFFER] Запущен RAW / KeeLoq сниффер")
        }

        binding.btnStopSniffer.setOnClickListener {
            bleManager.stopSniffer()
            appendConsole("[SNIFFER] Сниффер остановлен")
        }

        binding.btnClearSnifferList.setOnClickListener {
            snifferHistory.clear()
            renderSnifferList()
        }

        binding.btnReplaySnifferPacket.setOnClickListener {
            if (selectedSnifferIdx in snifferHistory.indices) {
                val entry = snifferHistory[selectedSnifferIdx]
                val item = entry.item
                if (item.type == SignalItem.SIG_KIND_RAW && item.pulses.isNotEmpty()) {
                    bleManager.transmitRaw(item.freq, 10, 5, item.pulses)
                } else {
                    bleManager.transmitFixed(item.freq, item.code, item.bitLen, item.pulseLen, item.protocol, 10, 7)
                }
                appendConsole("[TX REPLAY] Повторная передача пойманного пакета ${item.codeHex()}")
            }
        }

        binding.btnSaveSnifferPacket.setOnClickListener {
            if (selectedSnifferIdx in snifferHistory.indices) {
                val entry = snifferHistory[selectedSnifferIdx]
                val item = entry.item
                if (item.type == SignalItem.SIG_KIND_RAW && item.pulses.isNotEmpty()) {
                    bleManager.saveRaw("Captured_RAW", item.freq, 5, item.protoName, item.isKeeloq, item.keeloqSerial, item.keeloqBtn, item.keeloqEnc, item.pulses)
                } else {
                    bleManager.saveFixed("Captured_Fix", item.freq, item.code, item.bitLen, item.pulseLen, item.protocol, 5, item.protoName)
                }
                appendConsole("[SAVE] Сохранение пойманного пакета в NVS...")
            }
        }
    }

    private fun renderSnifferList() {
        binding.llSnifferPackets.removeAllViews()
        binding.tvSnifferCount.text = "ПОЙМАННЫЕ ПАКЕТЫ (${snifferHistory.size})"

        for (i in snifferHistory.indices.reversed()) {
            val entry = snifferHistory[i]
            val card = ItemSnifferPacketBinding.inflate(LayoutInflater.from(this), binding.llSnifferPackets, false)
            card.tvPacketTime.text = entry.timeStr
            card.tvPacketProto.text = entry.item.typeName()
            card.tvPacketRssi.text = "${entry.rssi} dBm"
            card.tvPacketFreq.text = String.format(Locale.US, "%.3f МГц", entry.item.freq)
            card.tvPacketCode.text = if (entry.item.isKeeloq) {
                "KeeLoq SN:${entry.item.keeloqSerialHex()} Btn:${entry.item.keeloqBtn}"
            } else if (entry.item.type == SignalItem.SIG_KIND_RAW) {
                "${entry.item.pulses.size} имп."
            } else {
                "${entry.item.codeHex()} (${entry.item.bitLen}b, TE=${entry.item.pulseLen}µs)"
            }

            val isSelected = (i == selectedSnifferIdx)
            card.root.setBackgroundResource(if (isSelected) R.drawable.square_btn_primary else R.drawable.square_btn_bg)
            if (isSelected) {
                card.tvPacketCode.setTextColor(Color.BLACK)
            }

            card.root.setOnClickListener {
                selectedSnifferIdx = i
                renderSnifferList()
                showSelectedSnifferDetails(entry)
            }
            binding.llSnifferPackets.addView(card.root)
        }

        if (selectedSnifferIdx in snifferHistory.indices) {
            showSelectedSnifferDetails(snifferHistory[selectedSnifferIdx])
        }
    }

    private fun showSelectedSnifferDetails(entry: SnifferEntry) {
        binding.cardSnifferDetails.visibility = View.VISIBLE
        val item = entry.item
        val infoStr = if (item.isKeeloq) {
            "[${entry.timeStr}] KeeLoq | SN: ${item.keeloqSerialHex()} | Кнопка: ${item.keeloqBtn} | Enc: ${item.keeloqEncHex()} | RSSI: ${entry.rssi} dBm"
        } else {
            "[${entry.timeStr}] ${item.typeName()} | ${item.codeHex()} | RSSI: ${entry.rssi} dBm"
        }
        binding.tvSniffPacketTitle.text = infoStr
        binding.waveformSniffer.setPulses(item.toWaveformPulses())
    }

    // --- TAB 5: BLE CONSOLE ---
    private fun setupConsoleTab() {
        binding.btnClearConsole.setOnClickListener {
            binding.tvConsoleLog.text = ""
        }
        binding.btnConsolePing.setOnClickListener {
            bleManager.requestPong()
        }
        binding.btnConsoleReboot.setOnClickListener {
            bleManager.rebootDevice()
        }
        binding.btnSendConsoleCmd.setOnClickListener {
            val cmd = binding.etConsoleCmd.text.toString().trim()
            if (cmd.isNotEmpty()) {
                bleManager.sendCommand(cmd)
                appendConsole("> $cmd")
                binding.etConsoleCmd.setText("")
            }
        }
    }

    private fun appendConsole(msg: String, isError: Boolean = false) {
        val t = timeFormat.format(Date())
        val prefix = if (isError) "[!] " else ""
        binding.tvConsoleLog.append("[$t] $prefix$msg\n")
        binding.scrollConsole.post { binding.scrollConsole.fullScroll(View.FOCUS_DOWN) }
    }

    // --- SIGNAL EDIT / ADD DIALOG ---
    private fun openSignalEditDialog(targetItem: SignalItem, isNew: Boolean) {
        val dialogBinding = DialogEditSignalBinding.inflate(layoutInflater)
        val dialog = AlertDialog.Builder(this)
            .setView(dialogBinding.root)
            .create()

        dialogBinding.tvDialogTitle.text = if (isNew) "СОЗДАНИЕ НОВОГО СИГНАЛА" else "РЕДАКТИРОВАНИЕ СИГНАЛА #${targetItem.index + 1}"
        dialogBinding.etDialogName.setText(targetItem.name)

        val typeOptions = arrayOf("Фиксированный код", "RAW Импульсы")
        val adType = ArrayAdapter(this, R.layout.spinner_item, typeOptions).apply { setDropDownViewResource(R.layout.spinner_dropdown_item) }
        val adFreq = ArrayAdapter(this, R.layout.spinner_item, freqs).apply { setDropDownViewResource(R.layout.spinner_dropdown_item) }
        val adProto = ArrayAdapter(this, R.layout.spinner_item, fixedProtocols).apply { setDropDownViewResource(R.layout.spinner_dropdown_item) }

        dialogBinding.spDialogType.adapter = adType
        dialogBinding.spDialogFreq.adapter = adFreq
        dialogBinding.spDialogProto.adapter = adProto

        dialogBinding.spDialogType.setSelection(if (targetItem.type == SignalItem.SIG_KIND_RAW) 1 else 0)
        dialogBinding.etDialogHex.setText(targetItem.codeHex())
        dialogBinding.etDialogDec.setText(targetItem.code.toString())
        dialogBinding.etDialogBitLen.setText(targetItem.bitLen.toString())
        dialogBinding.etDialogPulseLen.setText(targetItem.pulseLen.toString())
        dialogBinding.etDialogRepeats.setText(targetItem.repeats.toString())

        if (targetItem.pulses.isNotEmpty()) {
            dialogBinding.etDialogRawPulses.setText(targetItem.pulses.joinToString(" "))
        }

        fun updateDialogWaveform() {
            val isRaw = dialogBinding.spDialogType.selectedItemPosition == 1
            if (isRaw) {
                val tokens = dialogBinding.etDialogRawPulses.text.toString().split("[\\s,]+".toRegex()).filter { it.isNotEmpty() }
                val pulses = tokens.mapNotNull { it.toIntOrNull()?.let(::abs) }
                dialogBinding.waveformDialog.setPulses(pulses)
            } else {
                val code = dialogBinding.etDialogHex.text.toString().removePrefix("0x").removePrefix("0X").toLongOrNull(16) ?: 0L
                val bitLen = dialogBinding.etDialogBitLen.text.toString().toIntOrNull() ?: 24
                val te = dialogBinding.etDialogPulseLen.text.toString().toIntOrNull() ?: 320
                val proto = fixedProtocols[dialogBinding.spDialogProto.selectedItemPosition]
                val dummy = SignalItem(code = code, bitLen = bitLen, pulseLen = te, protoName = proto)
                dialogBinding.waveformDialog.setPulses(dummy.toWaveformPulses())
            }
        }

        dialogBinding.spDialogType.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                dialogBinding.llDialogFixedFields.visibility = if (position == 0) View.VISIBLE else View.GONE
                dialogBinding.llDialogRawFields.visibility = if (position == 1) View.VISIBLE else View.GONE
                updateDialogWaveform()
            }
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }

        dialogBinding.btnDialogTestTx.setOnClickListener {
            val f = freqValues[dialogBinding.spDialogFreq.selectedItemPosition]
            val isRaw = dialogBinding.spDialogType.selectedItemPosition == 1
            if (isRaw) {
                val tokens = dialogBinding.etDialogRawPulses.text.toString().split("[\\s,]+".toRegex()).filter { it.isNotEmpty() }
                val pulses = tokens.mapNotNull { it.toIntOrNull()?.let(::abs) }
                if (pulses.isNotEmpty()) bleManager.transmitRaw(f, 10, 5, pulses)
            } else {
                val code = dialogBinding.etDialogHex.text.toString().removePrefix("0x").removePrefix("0X").toLongOrNull(16) ?: 0L
                val bitLen = dialogBinding.etDialogBitLen.text.toString().toIntOrNull() ?: 24
                val te = dialogBinding.etDialogPulseLen.text.toString().toIntOrNull() ?: 320
                val protoIdx = dialogBinding.spDialogProto.selectedItemPosition + 1
                bleManager.transmitFixed(f, code, bitLen, te, protoIdx, 10, 5)
            }
        }

        dialogBinding.btnDialogSave.setOnClickListener {
            val name = dialogBinding.etDialogName.text.toString().trim().ifEmpty { "SavedSig" }
            val f = freqValues[dialogBinding.spDialogFreq.selectedItemPosition]
            val isRaw = dialogBinding.spDialogType.selectedItemPosition == 1
            val reps = dialogBinding.etDialogRepeats.text.toString().toIntOrNull() ?: 5

            if (isRaw) {
                val tokens = dialogBinding.etDialogRawPulses.text.toString().split("[\\s,]+".toRegex()).filter { it.isNotEmpty() }
                val pulses = tokens.mapNotNull { it.toIntOrNull()?.let(::abs) }
                if (pulses.isEmpty()) {
                    Toast.makeText(this, "Введите импульсы RAW", Toast.LENGTH_SHORT).show()
                    return@setOnClickListener
                }
                bleManager.saveRaw(name, f, reps, "RAW", false, 0, 0, 0, pulses)
            } else {
                val code = dialogBinding.etDialogHex.text.toString().removePrefix("0x").removePrefix("0X").toLongOrNull(16) ?: 0L
                val bitLen = dialogBinding.etDialogBitLen.text.toString().toIntOrNull() ?: 24
                val te = dialogBinding.etDialogPulseLen.text.toString().toIntOrNull() ?: 320
                val protoIdx = dialogBinding.spDialogProto.selectedItemPosition + 1
                val protoName = fixedProtocols[dialogBinding.spDialogProto.selectedItemPosition]
                bleManager.saveFixed(name, f, code, bitLen, te, protoIdx, reps, protoName)
            }
            dialog.dismiss()
        }

        dialogBinding.btnDialogCancel.setOnClickListener { dialog.dismiss() }
        updateDialogWaveform()
        dialog.show()
    }

    // --- BLE CALLBACKS ---
    override fun onDeviceFound(device: android.bluetooth.BluetoothDevice, name: String, rssi: Int) {
        binding.cardFoundDevice.visibility = View.VISIBLE
        binding.tvFoundDeviceName.text = name
        binding.tvFoundDeviceMac.text = "MAC: ${device.address} • Nordic UART BLE"
        val rssiQuality = when {
            rssi >= -60 -> "ОТЛИЧНЫЙ"
            rssi >= -75 -> "ХОРОШИЙ"
            else -> "СЛАБЫЙ"
        }
        binding.tvFoundRssi.text = "$rssi dBm [$rssiQuality]"
        binding.tvConnStep1.text = "[ ✔ ] 1. BLE радиомаяк $name обнаружен ($rssi dBm)"
        binding.tvConnStep1.setTextColor(Color.parseColor("#00FF66"))
        binding.tvScanStatus.text = "[ НАЙДЕНО: $name. СОПРЯЖЕНИЕ... ]"
        binding.btnConnectFound.isEnabled = true
        binding.btnConnectFound.text = "[ ПОДКЛЮЧИТЬ УСТРОЙСТВО ]"
    }

    override fun onConnectionStep(step: Int, desc: String) {
        when (step) {
            1 -> {
                binding.tvConnStep1.text = "[ ✔ ] 1. $desc"
                binding.tvConnStep1.setTextColor(Color.parseColor("#00FF66"))
            }
            2 -> {
                if (desc.contains("GATT") || desc.contains("MTU") || desc.contains("согласован") || desc.contains("настроен")) {
                    binding.tvConnStep2.text = "[ ✔ ] 2. $desc"
                    binding.tvConnStep2.setTextColor(Color.parseColor("#00FF66"))
                } else {
                    binding.tvConnStep2.text = "[ ⟳ ] 2. $desc"
                    binding.tvConnStep2.setTextColor(Color.WHITE)
                }
            }
            3 -> {
                if (desc.contains("завершена") || desc.contains("Готово") || desc.contains("готов")) {
                    binding.tvConnStep3.text = "[ ✔ ] 3. $desc"
                    binding.tvConnStep3.setTextColor(Color.parseColor("#00FF66"))
                    binding.tvScanStatus.text = "[ УСТРОЙСТВО ПОДКЛЮЧЕНО ]"
                    binding.pbScan.visibility = View.GONE
                    binding.root.postDelayed({
                        showMainStudio()
                    }, 400)
                } else {
                    binding.tvConnStep3.text = "[ ⟳ ] 3. $desc"
                    binding.tvConnStep3.setTextColor(Color.WHITE)
                }
            }
        }
    }

    override fun onConnectionStateChange(connected: Boolean, deviceName: String) {
        if (connected) {
            isOfflineMode = false
            binding.tvStatus.text = "[ BLE: $deviceName ]"
            binding.tvStatus.setTextColor(Color.parseColor("#00FF66"))
            binding.btnConnect.text = "[ СМЕНИТЬ ]"
            binding.tvConnStep2.text = "[ ✔ ] 2. Подключение GATT успешно"
            binding.tvConnStep2.setTextColor(Color.parseColor("#00FF66"))
        } else {
            binding.tvStatus.text = "[ BLE: ОТКЛЮЧЕНО ]"
            binding.tvStatus.setTextColor(ContextCompat.getColor(this, R.color.text_secondary))
            binding.tvTelemetry.text = "[ УСТРОЙСТВО НЕ НАЙДЕНО ]"
            binding.btnConnect.text = "[ ПОДКЛЮЧИТЬ ]"
            binding.btnConnectFound.isEnabled = true
            binding.btnConnectFound.text = "[ ПОДКЛЮЧИТЬ УСТРОЙСТВО ]"

            if (binding.layoutMainStudio.visibility == View.VISIBLE && !isOfflineMode) {
                // Связь была потеряна во время работы в студии - возвращаемся на экран сопряжения!
                showConnectScreen("[ СВЯЗЬ ПОТЕРЯНА. ПОИСК УСТРОЙСТВА... ]")
                resetConnectionSteps()
                binding.cardFoundDevice.visibility = View.GONE
                bleManager.startScan()
            }
        }
    }

    override fun onScanStateChange(scanning: Boolean) {
        if (scanning) {
            binding.pbScan.visibility = View.VISIBLE
            binding.tvScanStatus.text = "[ ⟳ ПОИСК GHOSTRF В ЭФИРЕ BLE... ]"
        } else {
            binding.pbScan.visibility = View.GONE
            if (!bleManager.isConnected() && binding.cardFoundDevice.visibility != View.VISIBLE) {
                binding.tvScanStatus.text = "[ ПОИСК ЗАВЕРШЕН. УСТРОЙСТВО НЕ НАЙДЕНО ]"
            }
        }
    }

    override fun onConnecting(deviceName: String) {
        binding.tvScanStatus.text = "[ ПОДКЛЮЧЕНИЕ К $deviceName... ]"
        binding.tvConnStep2.text = "[ ⟳ ] 2. Соединение GATT и согласование MTU 512..."
        binding.tvConnStep2.setTextColor(Color.WHITE)
    }

    override fun onLineReceived(line: String) {
        // Log all raw traffic in console tab
        if (!line.startsWith("{\"cmd\":\"SCREEN\"")) {
            appendConsole("< $line")
        }
    }

    override fun onScreenFrame(hex: String) {
        binding.oledView.updateFrameFromHex(hex)
        binding.chassisOledCanvas.updateFrameFromHex(hex)
    }

    override fun onPong(
        device: String, ver: String, chip: String, cc1101: Boolean,
        heap: Long, sigCount: Int, maxSig: Int, freq: Double, pa: Int, squelch: Int
    ) {
        val tele = String.format(Locale.US, "%s | %s v%s | CC1101: %s | Сигналов: %d/%d | %.3f МГц | Heap: %d",
            device, chip, ver, if (cc1101) "OK" else "ERR", sigCount, maxSig, freq, heap)
        binding.tvTelemetry.text = tele
        binding.tvConnStep3.text = "[ ✔ ] 3. Синхронизация завершена! Память: $sigCount/$maxSig"
        binding.tvConnStep3.setTextColor(Color.parseColor("#00FF66"))
        if (binding.layoutConnectScreen.visibility == View.VISIBLE) {
            binding.root.postDelayed({
                showMainStudio()
            }, 350)
        }
    }

    override fun onSignalsBegin(count: Int) {
        savedSignals.clear()
        appendConsole("[NVS] Загрузка списка ($count сигналов)...")
    }

    override fun onSignalItem(item: SignalItem) {
        savedSignals.add(item)
    }

    override fun onSignalsEnd() {
        appendConsole("[NVS OK] Загружено сигналов: ${savedSignals.size}")
        renderSignalsList()
    }

    override fun onTxDone(ok: Boolean, msg: String) {
        appendConsole(if (ok) "[OK] СИГНАЛ УСПЕШНО ПЕРЕДАН В ЭФИР" else "[ERR] Сбой передачи: $msg", !ok)
        Toast.makeText(this, if (ok) "Сигнал передан в эфир!" else "Ошибка передачи", Toast.LENGTH_SHORT).show()
    }

    override fun onSaveDone(ok: Boolean, count: Int) {
        appendConsole(if (ok) "[OK] Сигнал сохранен в NVS (всего: $count)" else "[ERR] Ошибка сохранения в NVS", !ok)
        bleManager.requestSignals()
    }

    override fun onDeleteDone(ok: Boolean, count: Int) {
        appendConsole(if (ok) "[OK] Сигнал удален (осталось: $count)" else "[ERR] Ошибка удаления", !ok)
        selectedSignalIdx = -1
        bleManager.requestSignals()
    }

    override fun onClearDone(ok: Boolean) {
        appendConsole(if (ok) "[OK] Все сигналы очищены" else "[ERR] Ошибка очистки", !ok)
        savedSignals.clear()
        selectedSignalIdx = -1
        renderSignalsList()
    }

    override fun onConfigReceived(freqIdx: Int, freqVal: Double, paIdx: Int, paVal: Int, squelchIdx: Int, squelchVal: Int) {
        if (freqIdx in freqs.indices) binding.spFreq.setSelection(freqIdx)
        if (paIdx in powers.indices) binding.spPower.setSelection(paIdx)
        if (squelchIdx in squelches.indices) binding.spSquelch.setSelection(squelchIdx)
        appendConsole(String.format(Locale.US, "[CFG] Загружена конфигурация: %.3f МГц, %d dBm, Squelch %d", freqVal, paVal, squelchVal))
    }

    override fun onCarrierDone(status: String) {
        isCarrierOn = (status == "ON")
        binding.btnToggleCarrier.text = if (isCarrierOn) "[ ВЫКЛЮЧИТЬ ПЕРЕДАЧУ НЕСУЩЕЙ ]" else "[ ВКЛЮЧИТЬ ПЕРЕДАЧУ НЕСУЩЕЙ ]"
    }

    override fun onSnifferStarted(mode: String) {
        appendConsole("[SNIFFER] Активен режим: $mode")
    }

    override fun onSnifferStopped() {
        appendConsole("[SNIFFER] Режим остановлен")
    }

    override fun onCapturedFixed(code: Long, codeHex: String, bitLen: Int, pulseLen: Int, proto: Int, freq: Double, rssi: Int) {
        val item = SignalItem(
            name = "Fixed_${codeHex.removePrefix("0x")}",
            type = SignalItem.SIG_KIND_FIXED,
            freq = freq,
            code = code,
            bitLen = bitLen,
            pulseLen = pulseLen,
            protocol = proto,
            protoName = if (proto == 1) "EV1527" else "Proto_$proto"
        )
        val entry = SnifferEntry(timeFormat.format(Date()), item, rssi)
        snifferHistory.add(entry)
        renderSnifferList()
        appendConsole("[RX FIXED] $codeHex ($code) | $bitLen bit, TE=$pulseLen мкс | RSSI: $rssi dBm | $freq МГц")
    }

    override fun onCapturedRaw(
        proto: String, isKeeloq: Boolean, serial: Long, btn: Int,
        enc: Long, count: Int, freq: Double, rssi: Int, pulses: List<Int>
    ) {
        val item = SignalItem(
            name = if (isKeeloq) "KeeLoq_$serial" else "RAW_$count",
            type = SignalItem.SIG_KIND_RAW,
            freq = freq,
            isKeeloq = isKeeloq,
            keeloqSerial = serial,
            keeloqBtn = btn,
            keeloqEnc = enc,
            rawCount = count,
            protoName = proto
        )
        item.pulses.addAll(pulses)
        val entry = SnifferEntry(timeFormat.format(Date()), item, rssi)
        snifferHistory.add(entry)
        renderSnifferList()
        val name = if (isKeeloq) "KeeLoq (SN:$serial, B:$btn)" else proto
        appendConsole("[RX RAW] $name | $count имп. | RSSI: $rssi dBm | $freq МГц")
    }

    override fun onDeviceReady(device: String, ver: String) {
        appendConsole("[DEVICE] $device готов (v$ver)")
        binding.tvConnStep3.text = "[ ✔ ] 3. Устройство $device готово (v$ver)"
        binding.tvConnStep3.setTextColor(Color.parseColor("#00FF66"))
        if (binding.layoutConnectScreen.visibility == View.VISIBLE) {
            binding.root.postDelayed({
                showMainStudio()
            }, 350)
        }
    }

    override fun onLog(msg: String, isError: Boolean) {
        appendConsole(msg)
    }

    private fun checkPermissionsAndStart() {
        val permissions = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions.add(Manifest.permission.BLUETOOTH_SCAN)
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        val needed = permissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (needed.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, needed.toTypedArray(), 101)
        } else {
            bleManager.startScan()
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 101 && grantResults.isNotEmpty() && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
            bleManager.startScan()
        } else {
            binding.tvScanStatus.text = "[ ТРЕБУЮТСЯ РАЗРЕШЕНИЯ BLUETOOTH ]"
            binding.pbScan.visibility = View.GONE
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        bleManager.disconnect()
    }
}
