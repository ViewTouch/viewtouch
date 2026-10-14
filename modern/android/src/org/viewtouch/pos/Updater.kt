package org.viewtouch.pos

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import androidx.core.content.FileProvider
import java.io.File

// An update the store sent (net/remote_session.cpp saved it in the cache):
// Android's installer asks whoever is here to install it. The first time,
// Android asks to allow ViewTouch to install apps; then Update again.
object Updater {
    private const val AUTHORITY = "org.viewtouch.pos.updates"

    @JvmStatic
    fun install(context: Context, path: String) {
        Handler(Looper.getMainLooper()).post {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && !context.packageManager.canRequestPackageInstalls()) {
                context.startActivity(
                    Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:" + context.packageName))
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                )
                return@post
            }
            val uri = FileProvider.getUriForFile(context, AUTHORITY, File(path))
            context.startActivity(
                Intent(Intent.ACTION_VIEW)
                    .setDataAndType(uri, "application/vnd.android.package-archive")
                    .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK)
            )
        }
    }
}
