package com.rfhub.studio

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View

class OledCanvasView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val oledWidth = 128
    private val oledHeight = 64
    private val buffer = ByteArray(1024)

    // Монолитная матрица 128x64
    private val pixelInts = IntArray(128 * 64)
    private val bitmap = Bitmap.createBitmap(128, 64, Bitmap.Config.ARGB_8888)
    private val srcRect = Rect(0, 0, 128, 64)
    private val destRect = RectF()

    var hasPixelGrid = true
        private set
    private var isInverted = false

    private val bgPaint = Paint().apply {
        color = Color.BLACK
        style = Paint.Style.FILL
    }
    private val bitmapPaint = Paint().apply {
        isFilterBitmap = false // Nearest-neighbor: строго ровные квадратные пиксели без дробного сглаживания
        isDither = false
    }
    private val borderPaint = Paint().apply {
        color = Color.parseColor("#333333")
        style = Paint.Style.STROKE
        strokeWidth = 2f
    }
    private val gridPaint = Paint().apply {
        color = Color.BLACK
        style = Paint.Style.STROKE
        strokeWidth = 1f
        isAntiAlias = false
    }

    private val pixelPaint = Paint().apply {
        color = Color.WHITE
        style = Paint.Style.FILL
        isAntiAlias = false
    }

    fun updateFrameFromHex(hexStr: String) {
        if (hexStr.length < 2048) return
        try {
            var sIdx = 0
            for (i in 0 until 1024) {
                val h = Character.digit(hexStr[sIdx++], 16)
                val l = Character.digit(hexStr[sIdx++], 16)
                buffer[i] = ((h shl 4) or l).toByte()
            }
            renderBufferToBitmap()
            postInvalidate()
        } catch (_: Exception) {}
    }

    private fun renderBufferToBitmap() {
        val onColor = if (isInverted) 0xFF000000.toInt() else 0xFFFFFFFF.toInt()
        val offColor = if (isInverted) 0xFFFFFFFF.toInt() else 0xFF000000.toInt()

        for (page in 0 until 8) {
            val pageOffset = page * 128
            val yBase = page * 8
            for (x in 0 until 128) {
                val b = buffer[pageOffset + x].toInt()
                for (bit in 0 until 8) {
                    val y = yBase + bit
                    val isPixelOn = (b and (1 shl bit)) != 0
                    pixelInts[y * 128 + x] = if (isPixelOn) onColor else offColor
                }
            }
        }
        bitmap.setPixels(pixelInts, 0, 128, 0, 0, 128, 64)
    }

    fun setInverted(inverted: Boolean) {
        isInverted = inverted
        pixelPaint.color = if (isInverted) Color.BLACK else Color.WHITE
        renderBufferToBitmap()
        invalidate()
    }

    fun toggleGrid(): Boolean {
        hasPixelGrid = !hasPixelGrid
        invalidate()
        return hasPixelGrid
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val w = width
        val h = height
        if (w <= 0 || h <= 0) return

        // Строгий целочисленный масштаб для исключения дробных искажений
        val pad = 4
        val availW = w - (pad * 2)
        val availH = h - (pad * 2)
        val scale = maxOf(1, minOf(availW / oledWidth, availH / oledHeight))

        val displayW = oledWidth * scale
        val displayH = oledHeight * scale

        val startX = (w - displayW) / 2
        val startY = (h - displayH) / 2

        // Фон корпуса
        canvas.drawRect(0f, 0f, w.toFloat(), h.toFloat(), bgPaint)

        // Область дисплея
        destRect.set(startX.toFloat(), startY.toFloat(), (startX + displayW).toFloat(), (startY + displayH).toFloat())

        if (hasPixelGrid && scale >= 3) {
            // РЕЖИМ ПИКСЕЛЬНОЙ МАТРИЦЫ:
            // Каждый активный пиксель рисуется строго одинаковым целым квадратом (scale - 1)
            // с математически равномерным зазором в ровно 1px фонового цвета между ними.
            val displayBgColor = if (isInverted) Color.WHITE else Color.BLACK
            bgPaint.color = displayBgColor
            canvas.drawRect(destRect, bgPaint)
            bgPaint.color = Color.BLACK // возврат фона корпуса

            val pixelSize = (scale - 1).toFloat()
            pixelPaint.color = if (isInverted) Color.BLACK else Color.WHITE

            for (page in 0 until 8) {
                val pageOffset = page * 128
                val yBase = page * 8
                for (x in 0 until 128) {
                    val b = buffer[pageOffset + x].toInt() and 0xFF
                    if (!isInverted && b == 0) continue
                    if (isInverted && b == 0xFF) continue

                    val px = (startX + x * scale).toFloat()
                    for (bit in 0 until 8) {
                        val isOn = ((b ushr bit) and 1) == 1
                        val shouldDraw = if (isInverted) !isOn else isOn
                        if (shouldDraw) {
                            val py = (startY + (yBase + bit) * scale).toFloat()
                            canvas.drawRect(px, py, px + pixelSize, py + pixelSize, pixelPaint)
                        }
                    }
                }
            }
        } else {
            // РЕЖИМ МОНОЛИТА:
            // Сплошное монолитное изображение без зазоров между пикселями
            canvas.drawBitmap(bitmap, srcRect, destRect, bitmapPaint)
        }

        // Внешняя аккуратная рамка дисплея
        canvas.drawRect(destRect, borderPaint)
    }
}
