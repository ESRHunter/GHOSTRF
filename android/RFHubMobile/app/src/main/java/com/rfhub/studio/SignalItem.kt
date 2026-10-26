package com.rfhub.studio

import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.abs
import kotlin.math.roundToLong

class SignalItem(
    var index: Int = -1,
    var name: String = "Signal",
    var type: Int = SIG_KIND_FIXED,
    var freq: Double = 433.92,
    var repeats: Int = 5,
    var protoName: String = "EV1527",
    var code: Long = 0L,
    var bitLen: Int = 24,
    var pulseLen: Int = 320,
    var protocol: Int = 1,
    var isKeeloq: Boolean = false,
    var keeloqSerial: Long = 0L,
    var keeloqBtn: Int = 0,
    var keeloqEnc: Long = 0L,
    var rawCount: Int = 0,
    val pulses: MutableList<Int> = mutableListOf()
) {
    companion object {
        const val SIG_KIND_FIXED = 0
        const val SIG_KIND_RAW = 1
        const val SIG_KIND_KLQ_GEN = 2
        const val SIG_KIND_SMART_EV1527 = 3

        fun fromJson(obj: JSONObject): SignalItem {
            val item = SignalItem(
                index = obj.optInt("index", -1),
                name = obj.optString("name", "Unnamed"),
                type = obj.optInt("type", SIG_KIND_FIXED),
                freq = obj.optDouble("freq", 433.92),
                repeats = obj.optInt("repeats", 5),
                protoName = obj.optString("proto", "EV1527"),
                code = obj.optLong("code", 0L),
                bitLen = obj.optInt("bitLen", 24),
                pulseLen = obj.optInt("pulseLen", 320),
                protocol = obj.optInt("protocol", 1),
                isKeeloq = obj.optBoolean("isKeeloq", false),
                keeloqSerial = obj.optLong("keeloqSerial", 0L),
                keeloqBtn = obj.optInt("keeloqBtn", 0),
                keeloqEnc = obj.optLong("keeloqEnc", 0L)
            )
            val pArr = obj.optJSONArray("pulses")
            if (pArr != null) {
                for (i in 0 until pArr.length()) {
                    item.pulses.add(pArr.getInt(i))
                }
            }
            item.rawCount = item.pulses.size
            return item
        }

        fun fromSubGhzFile(content: String, defaultName: String = "Imported"): SignalItem {
            val item = SignalItem(name = defaultName, type = SIG_KIND_RAW)
            val lines = content.split('\n')
            var currentProtocol = "RAW"

            for (rawLine in lines) {
                val line = rawLine.trim()
                if (line.isEmpty() || line.startsWith("#")) continue

                if (line.startsWith("Frequency:")) {
                    val hz = line.substringAfter(':').trim().toLongOrNull() ?: 0L
                    if (hz > 0) item.freq = hz.toDouble() / 1000000.0
                } else if (line.startsWith("Protocol:")) {
                    currentProtocol = line.substringAfter(':').trim()
                    item.protoName = currentProtocol
                    item.type = if (currentProtocol.equals("RAW", ignoreCase = true)) SIG_KIND_RAW else SIG_KIND_FIXED
                } else if (line.startsWith("Bit:")) {
                    item.bitLen = line.substringAfter(':').trim().toIntOrNull() ?: 24
                } else if (line.startsWith("Key:")) {
                    val keyStr = line.substringAfter(':').trim().replace(" ", "")
                    item.code = keyStr.toLongOrNull(16) ?: 0L
                } else if (line.startsWith("TE:")) {
                    item.pulseLen = line.substringAfter(':').trim().toIntOrNull() ?: 320
                } else if (line.startsWith("Repeat:")) {
                    item.repeats = line.substringAfter(':').trim().toIntOrNull() ?: 5
                } else if (line.startsWith("RAW_Data:")) {
                    item.type = SIG_KIND_RAW
                    val dataPart = line.substringAfter(':').trim()
                    val tokens = dataPart.split("\\s+".toRegex()).filter { it.isNotEmpty() }
                    for (tok in tokens) {
                        val v = tok.toIntOrNull() ?: continue
                        item.pulses.add(abs(v))
                    }
                } else if (item.type == SIG_KIND_RAW && !line.contains(':')) {
                    val tokens = line.split("\\s+".toRegex()).filter { it.isNotEmpty() }
                    for (tok in tokens) {
                        val v = tok.toIntOrNull() ?: continue
                        if (v != 0) item.pulses.add(abs(v))
                    }
                }
            }
            item.rawCount = item.pulses.size
            return item
        }
    }

    fun typeName(): String = when (type) {
        SIG_KIND_RAW -> if (isKeeloq) "KeeLoq RAW" else "RAW Импульсы"
        SIG_KIND_KLQ_GEN -> "Виртуальный KeeLoq"
        SIG_KIND_SMART_EV1527 -> "EV1527 Smart Remote"
        SIG_KIND_FIXED -> "$protoName (Фиксированный)"
        else -> protoName
    }

    fun codeHex(): String {
        val digits = if (bitLen <= 16) 4 else if (bitLen <= 24) 6 else 8
        return "0x" + code.toString(16).uppercase().padStart(digits, '0')
    }

    fun codeBin(): String {
        val total = if (bitLen > 0) bitLen else 24
        val rawBin = java.lang.Long.toBinaryString(code).padStart(total, '0')
        val sb = java.lang.StringBuilder()
        for (i in rawBin.indices) {
            if (i > 0 && (rawBin.length - i) % 4 == 0) sb.append(' ')
            sb.append(rawBin[i])
        }
        return sb.toString()
    }

    fun keeloqSerialHex(): String = "0x" + keeloqSerial.toString(16).uppercase().padStart(7, '0')
    fun keeloqEncHex(): String = "0x" + keeloqEnc.toString(16).uppercase().padStart(8, '0')

    fun toWaveformPulses(): List<Int> {
        if (pulses.isNotEmpty()) return pulses

        val synthetic = mutableListOf<Int>()
        val te = if (pulseLen > 0) pulseLen else 320

        if (protoName.contains("Came", ignoreCase = true)) {
            synthetic.add(te)
            synthetic.add(te * 36)
            for (i in 11 downTo 0) {
                val bit = ((code shr i) and 1L) == 1L
                if (bit) {
                    synthetic.add(te * 2)
                    synthetic.add(te)
                } else {
                    synthetic.add(te)
                    synthetic.add(te * 2)
                }
            }
        } else if (protoName.contains("Nice", ignoreCase = true)) {
            synthetic.add(te * 2)
            synthetic.add(te * 24)
            for (i in 11 downTo 0) {
                val bit = ((code shr i) and 1L) == 1L
                if (bit) {
                    synthetic.add(te * 2)
                    synthetic.add(te)
                } else {
                    synthetic.add(te)
                    synthetic.add(te * 2)
                }
            }
        } else {
            // Princeton / EV1527 / PT2262 / RC-Switch 1
            synthetic.add(te * 1)
            synthetic.add(te * 31)
            val totalBits = if (bitLen > 0) bitLen else 24
            for (i in totalBits - 1 downTo 0) {
                val bit = ((code shr i) and 1L) == 1L
                if (bit) {
                    synthetic.add(te * 3)
                    synthetic.add(te * 1)
                } else {
                    synthetic.add(te * 1)
                    synthetic.add(te * 3)
                }
            }
        }
        return synthetic
    }

    fun toJson(): JSONObject {
        val obj = JSONObject()
        obj.put("index", index)
        obj.put("name", name)
        obj.put("type", type)
        obj.put("freq", freq)
        obj.put("repeats", repeats)
        obj.put("proto", protoName)
        obj.put("code", code)
        obj.put("bitLen", bitLen)
        obj.put("pulseLen", pulseLen)
        obj.put("protocol", protocol)
        obj.put("isKeeloq", isKeeloq)
        obj.put("keeloqSerial", keeloqSerial)
        obj.put("keeloqBtn", keeloqBtn)
        obj.put("keeloqEnc", keeloqEnc)
        obj.put("rawCount", pulses.size)

        val pArr = JSONArray()
        for (p in pulses) pArr.put(p)
        obj.put("pulses", pArr)
        return obj
    }

    fun toSubGhzFile(): String {
        val sb = java.lang.StringBuilder()
        sb.append("Filetype: Flipper SubGhz Key File\n")
        sb.append("Version: 1\n")
        val hz = (freq * 1000000.0).roundToLong()
        sb.append("Frequency: $hz\n")
        sb.append("Preset: FuriHalSubGhzPresetOok650Async\n")

        if (type == SIG_KIND_RAW || pulses.isNotEmpty()) {
            sb.append("Protocol: RAW\n")
            sb.append("RAW_Data:")
            for (i in pulses.indices) {
                if (i % 24 == 0) sb.append("\n")
                val sign = if (i % 2 == 0) 1 else -1
                sb.append(" ").append(sign * pulses[i])
            }
            sb.append("\n")
        } else {
            sb.append("Protocol: ").append(if (protoName.isEmpty()) "Princeton" else protoName).append("\n")
            sb.append("Bit: ").append(if (bitLen > 0) bitLen else 24).append("\n")
            sb.append("Key: ").append(code.toString(16).uppercase().padStart(16, '0')).append("\n")
            sb.append("TE: ").append(if (pulseLen > 0) pulseLen else 320).append("\n")
            sb.append("Repeat: ").append(if (repeats > 0) repeats else 5).append("\n")
        }
        return sb.toString()
    }
}
