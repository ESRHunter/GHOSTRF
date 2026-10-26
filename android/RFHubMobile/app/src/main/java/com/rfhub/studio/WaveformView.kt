package com.rfhub.studio

import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.View
import kotlin.math.max
import kotlin.math.min

class WaveformView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val pulses = mutableListOf<Int>()
    private var firstLevelHigh = true
    private var isInverted = false

    private var zoom = 1.0f
    private var scrollXOffset = 0.0f
    private var totalDurationUs = 0L

    private val bgPaint = Paint().apply {
        color = Color.parseColor("#080808")
        style = Paint.Style.FILL
    }
    private val borderPaint = Paint().apply {
        color = Color.parseColor("#282828")
        style = Paint.Style.STROKE
        strokeWidth = 2f
    }
    private val gridPaint = Paint().apply {
        color = Color.parseColor("#181818")
        style = Paint.Style.STROKE
        strokeWidth = 1f
    }
    private val wavePaint = Paint().apply {
        color = Color.parseColor("#00FF66") // Cyber Green
        style = Paint.Style.STROKE
        strokeWidth = 3f
        strokeJoin = Paint.Join.MITER
        isAntiAlias = true
    }
    private val fillPaint = Paint().apply {
        color = Color.parseColor("#1200FF66")
        style = Paint.Style.FILL
    }
    private val textPaint = Paint().apply {
        color = Color.parseColor("#888888")
        textSize = 28f
        isAntiAlias = true
        typeface = Typeface.MONOSPACE
    }

    private val path = Path()
    private val fillPath = Path()

    private val scaleDetector = ScaleGestureDetector(context, object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
        override fun onScale(detector: ScaleGestureDetector): Boolean {
            zoom = (zoom * detector.scaleFactor).coerceIn(1.0f, 60.0f)
            clampScroll()
            invalidate()
            return true
        }
    })

    private val gestureDetector = GestureDetector(context, object : GestureDetector.SimpleOnGestureListener() {
        override fun onScroll(e1: MotionEvent?, e2: MotionEvent, distanceX: Float, distanceY: Float): Boolean {
            scrollXOffset += distanceX
            clampScroll()
            invalidate()
            return true
        }
    })

    fun setPulses(newPulses: List<Int>, firstHigh: Boolean = true) {
        pulses.clear()
        pulses.addAll(newPulses)
        firstLevelHigh = firstHigh
        totalDurationUs = pulses.sumOf { it.toLong() }
        zoom = 1.0f
        scrollXOffset = 0.0f
        invalidate()
    }

    fun clear() {
        pulses.clear()
        totalDurationUs = 0L
        zoom = 1.0f
        scrollXOffset = 0.0f
        invalidate()
    }

    fun zoomIn() {
        zoom = min(60.0f, zoom * 1.35f)
        clampScroll()
        invalidate()
    }

    fun zoomOut() {
        zoom = max(1.0f, zoom / 1.35f)
        clampScroll()
        invalidate()
    }

    fun resetZoom() {
        zoom = 1.0f
        scrollXOffset = 0.0f
        invalidate()
    }

    fun toggleInvert(): Boolean {
        isInverted = !isInverted
        invalidate()
        return isInverted
    }

    private fun clampScroll() {
        val w = width.toFloat()
        val totalWidth = w * zoom
        val maxScroll = max(0.0f, totalWidth - w)
        scrollXOffset = scrollXOffset.coerceIn(0.0f, maxScroll)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        var handled = scaleDetector.onTouchEvent(event)
        handled = gestureDetector.onTouchEvent(event) || handled
        return handled || super.onTouchEvent(event)
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val w = width.toFloat()
        val h = height.toFloat()
        if (w <= 0 || h <= 0) return

        canvas.drawRect(0f, 0f, w, h, bgPaint)
        canvas.drawRect(1f, 1f, w - 1f, h - 1f, borderPaint)

        // Center line
        canvas.drawLine(0f, h / 2f, w, h / 2f, gridPaint)

        if (pulses.isEmpty() || totalDurationUs <= 0) {
            val emptyText = "[ НЕТ ДАННЫХ ДЛЯ ОТОБРАЖЕНИЯ ФОРМЫ СИГНАЛА ]"
            val textW = textPaint.measureText(emptyText)
            canvas.drawText(emptyText, max(16f, (w - textW) / 2f), h / 2f + 10f, textPaint)
            return
        }

        val topY = h * 0.22f
        val botY = h * 0.78f

        path.reset()
        fillPath.reset()

        val plotWidth = w * zoom
        var curUs = 0L
        var isHigh = if (isInverted) !firstLevelHigh else firstLevelHigh

        var startX = -scrollXOffset
        var startY = if (isHigh) topY else botY

        path.moveTo(startX, startY)
        fillPath.moveTo(startX, botY)
        fillPath.lineTo(startX, startY)

        for (dur in pulses) {
            val endUs = curUs + dur
            val endX = (endUs.toFloat() / totalDurationUs.toFloat()) * plotWidth - scrollXOffset
            val y = if (isHigh) topY else botY

            path.lineTo(endX, y)
            fillPath.lineTo(endX, y)

            // Переход
            isHigh = !isHigh
            val nextY = if (isHigh) topY else botY
            path.lineTo(endX, nextY)
            fillPath.lineTo(endX, nextY)

            curUs = endUs
        }

        fillPath.lineTo(curUs.toFloat() / totalDurationUs.toFloat() * plotWidth - scrollXOffset, botY)
        fillPath.close()

        canvas.drawPath(fillPath, fillPaint)
        canvas.drawPath(path, wavePaint)

        // Текстовая плашка телеметрии
        val durationMs = totalDurationUs / 1000.0
        val infoText = String.format("%.2f мс | %d имп. | Zoom: %.1fx", durationMs, pulses.size, zoom)
        canvas.drawText(infoText, 16f, 36f, textPaint)
    }
}
