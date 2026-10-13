package org.viewtouch.pos

import android.app.Activity
import android.os.Bundle
import com.stripe.stripeterminal.TerminalApplicationDelegate
import org.qtproject.qt.android.bindings.QtApplication

// Qt's application, telling the Stripe Terminal SDK about the app's
// lifecycle (required for Apps on Devices on a Stripe smart reader).
class ViewTouchApplication : QtApplication() {
    override fun onCreate() {
        super.onCreate()
        TerminalApplicationDelegate.onCreate(this)
        // A tablet only for ViewTouch: pinned whenever it's in front (Kiosk.kt).
        registerActivityLifecycleCallbacks(object : ActivityLifecycleCallbacks {
            override fun onActivityResumed(activity: Activity) = Kiosk.apply(activity)
            override fun onActivityCreated(activity: Activity, state: Bundle?) {}
            override fun onActivityStarted(activity: Activity) {}
            override fun onActivityPaused(activity: Activity) {}
            override fun onActivityStopped(activity: Activity) {}
            override fun onActivitySaveInstanceState(activity: Activity, state: Bundle) {}
            override fun onActivityDestroyed(activity: Activity) {}
        })
    }
}
