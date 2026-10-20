package org.viewtouch.pos

import android.content.Context
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager

// A short buzz for each item added (the server knows it went on without
// looking), two for one that didn't (look at the screen). From
// LayoutController::buzz; quiet on a device without a vibrator.
object Haptics {
    @JvmStatic
    @Suppress("DEPRECATION")   // the older ways, for older Androids
    fun buzz(context: Context, refused: Boolean) {
        val vibrator: Vibrator? = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            (context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as? VibratorManager)?.defaultVibrator
        else
            context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
        if (vibrator == null || !vibrator.hasVibrator())
            return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            vibrator.vibrate(VibrationEffect.createPredefined(
                if (refused) VibrationEffect.EFFECT_DOUBLE_CLICK else VibrationEffect.EFFECT_CLICK))
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            vibrator.vibrate(if (refused) VibrationEffect.createWaveform(longArrayOf(0, 40, 80, 40), -1)
                             else VibrationEffect.createOneShot(25, VibrationEffect.DEFAULT_AMPLITUDE))
        } else if (refused) {
            vibrator.vibrate(longArrayOf(0, 40, 80, 40), -1)
        } else {
            vibrator.vibrate(25)
        }
    }
}
