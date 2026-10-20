package org.viewtouch.pos

import android.content.Context
import android.os.Handler
import android.os.Looper
import com.stripe.stripeterminal.Terminal
import com.stripe.stripeterminal.external.callable.AppsOnDevicesListener
import com.stripe.stripeterminal.external.callable.Callback
import com.stripe.stripeterminal.external.callable.Cancelable
import com.stripe.stripeterminal.external.callable.ConnectionTokenCallback
import com.stripe.stripeterminal.external.callable.ConnectionTokenProvider
import com.stripe.stripeterminal.external.callable.DiscoveryListener
import com.stripe.stripeterminal.external.callable.OfflineListener
import com.stripe.stripeterminal.external.callable.PaymentIntentCallback
import com.stripe.stripeterminal.external.callable.ReaderCallback
import com.stripe.stripeterminal.external.callable.TerminalListener
import com.stripe.stripeterminal.external.models.CaptureMethod
import com.stripe.stripeterminal.external.models.CardPresentParameters
import com.stripe.stripeterminal.external.models.PaymentMethodOptionsParameters
import com.stripe.stripeterminal.external.models.CollectPaymentIntentConfiguration
import com.stripe.stripeterminal.external.models.ConfirmPaymentIntentConfiguration
import com.stripe.stripeterminal.external.models.SurchargeConfiguration
import com.stripe.stripeterminal.external.models.SurchargeConsent
import com.stripe.stripeterminal.external.models.SurchargeConsentCollection
import com.stripe.stripeterminal.external.models.ConnectionConfiguration
import com.stripe.stripeterminal.external.models.ConnectionStatus
import com.stripe.stripeterminal.external.models.ConnectionTokenException
import com.stripe.stripeterminal.external.models.CreateConfiguration
import com.stripe.stripeterminal.external.models.OfflineBehavior
import com.stripe.stripeterminal.external.models.OfflineStatus
import com.stripe.stripeterminal.external.models.PaymentIntentStatus
import com.stripe.stripeterminal.external.models.DisconnectReason
import com.stripe.stripeterminal.external.models.DiscoveryConfiguration
import com.stripe.stripeterminal.external.models.LocaleConfig
import com.stripe.stripeterminal.external.models.PaymentIntent
import com.stripe.stripeterminal.external.models.PaymentIntentParameters
import com.stripe.stripeterminal.external.models.Reader
import com.stripe.stripeterminal.external.models.ReaderEvent
import com.stripe.stripeterminal.external.models.TerminalException
import com.stripe.stripeterminal.log.LogLevel
import org.json.JSONObject
import java.util.UUID

// ViewTouch running on a Stripe smart reader (Apps on Devices): connects to
// the reader it runs on, and takes a card for an amount. Stripe's own app
// shows the payment screens; card data never reaches ViewTouch. The C++
// side (ui/cpp/cardreader.cpp) calls the @JvmStatic functions and hears back
// through the native ones. Connection tokens come from the store's computer,
// which holds the Stripe secret key.
object StripeBridge {
    @JvmStatic external fun nativeNeedToken()
    @JvmStatic external fun nativeStatus(text: String, ready: Boolean)
    @JvmStatic external fun nativeResult(json: String)
    @JvmStatic external fun nativeForwarded(json: String)

    private val main = Handler(Looper.getMainLooper())
    private var pendingToken: ConnectionTokenCallback? = null
    private var collecting: Cancelable? = null
    private var current: PaymentIntent? = null
    private var currentRef = ""   // ViewTouch's id for the payment (metadata viewtouch_payment)
    // This payment: what was asked (cents, tip included), the surcharge a
    // credit card adds and what the guest is told; then the card's kind and
    // the surcharge it paid.
    private var requested = 0L
    private var surchargeOffered = 0L
    private var surchargeNotice = ""
    private var funding = ""
    private var surchargePaid = 0L
    private var connecting = false

    private val tokens = object : ConnectionTokenProvider {
        override fun fetchConnectionToken(callback: ConnectionTokenCallback) {
            pendingToken = callback
            nativeNeedToken()
        }
    }

    // Cards taken while the internet was down: each one reaching Stripe (or
    // declined) is told to the store, by the id ViewTouch gave it.
    private val offline = object : OfflineListener {
        override fun onOfflineStatusChange(offlineStatus: OfflineStatus) {
            val waiting = offlineStatus.reader?.offlinePaymentsCount ?: offlineStatus.sdk.offlinePaymentsCount
            if (waiting > 0)
                nativeStatus("Card reader ready · $waiting card(s) waiting to reach Stripe", true)
        }

        override fun onPaymentIntentForwarded(paymentIntent: PaymentIntent, e: TerminalException?) {
            val ref = paymentIntent.metadata?.get("viewtouch_payment") ?: return
            val ok = e == null && paymentIntent.id != null && paymentIntent.status in
                setOf(PaymentIntentStatus.SUCCEEDED, PaymentIntentStatus.REQUIRES_CAPTURE, PaymentIntentStatus.PROCESSING)
            nativeForwarded(
                JSONObject()
                    .put("localRef", ref)
                    .put("id", paymentIntent.id ?: "")
                    .put("status", if (ok) "ok" else "declined")
                    .put("error", e?.errorMessage ?: paymentIntent.status?.name ?: "")
                    .toString()
            )
        }

        override fun onForwardingFailure(e: TerminalException) {
            nativeStatus("Card reader: couldn't send a card to Stripe yet (${e.errorMessage})", true)
        }
    }

    private val listener = object : TerminalListener {
        override fun onConnectionStatusChange(status: ConnectionStatus) {
            if (status == ConnectionStatus.CONNECTED)
                nativeStatus("Card reader ready", true)
        }
    }

    @JvmStatic
    fun start(context: Context) {
        main.post {
            try {
                if (!Terminal.isInitialized()) {
                    Terminal.init(
                        context = context.applicationContext,
                        logLevel = LogLevel.NONE,
                        tokenProvider = tokens,
                        listener = listener,
                        offlineListener = offline,
                        localeConfig = LocaleConfig.CardLanguagePreferenceIfAvailable,
                    )
                }
                connect()
            } catch (e: Throwable) {
                nativeStatus("Card reader: ${e.message}", false)
            }
        }
    }

    // The token the store's computer got from Stripe (or why it couldn't).
    @JvmStatic
    fun provideToken(token: String, error: String) {
        main.post {
            val callback = pendingToken ?: return@post
            pendingToken = null
            if (token.isNotEmpty())
                callback.onSuccess(token)
            else
                callback.onFailure(ConnectionTokenException(error.ifEmpty { "No connection token" }))
        }
    }

    private fun retryLater(why: String) {
        connecting = false
        nativeStatus("Card reader: $why", false)
        main.postDelayed({ connect() }, 10_000)
    }

    private fun connect() {
        if (connecting || Terminal.getInstance().connectedReader != null)
            return
        connecting = true
        nativeStatus("Connecting to the card reader…", false)
        var found = false
        Terminal.getInstance().discoverReaders(
            DiscoveryConfiguration.AppsOnDevicesDiscoveryConfiguration(),
            object : DiscoveryListener {
                override fun onUpdateDiscoveredReaders(readers: List<Reader>) {
                    val reader = readers.firstOrNull() ?: return
                    if (found)
                        return
                    found = true
                    Terminal.getInstance().connectReader(
                        reader,
                        ConnectionConfiguration.AppsOnDevicesConnectionConfiguration(
                            object : AppsOnDevicesListener {
                                override fun onDisconnect(reason: DisconnectReason) {
                                    nativeStatus("Card reader disconnected", false)
                                    main.postDelayed({ connect() }, 3_000)
                                }

                                override fun onReportReaderEvent(event: ReaderEvent) {}
                            }
                        ),
                        object : ReaderCallback {
                            override fun onSuccess(reader: Reader) {
                                connecting = false
                                nativeStatus("Card reader ready", true)
                            }

                            override fun onFailure(e: TerminalException) = retryLater(e.errorMessage)
                        }
                    )
                }
            },
            object : Callback {
                override fun onSuccess() {}
                override fun onFailure(e: TerminalException) = retryLater(e.errorMessage)
            }
        )
    }

    // Take a card for `amount` (cents, tip included). With the internet down,
    // it's kept and sent later, up to the store's limits: `offlineMax` a card
    // and `offlineTotal` waiting in all (0: never offline). `hold`: a bar
    // tab's card, only set aside (manual capture; the store's computer
    // charges it when the tab closes, raising the hold if need be).
    @JvmStatic
    fun charge(amount: Long, currency: String, description: String, checkRef: String, offlineMax: Long, offlineTotal: Long,
               hold: Boolean, surcharge: Long, notice: String) {
        main.post {
            val localRef = UUID.randomUUID().toString()
            requested = amount
            surchargeOffered = if (hold) 0L else surcharge
            surchargeNotice = notice
            funding = ""
            surchargePaid = 0L
            val builder = PaymentIntentParameters.Builder()
                .setAmount(amount)
                .setCurrency(currency)
                .setCaptureMethod(if (hold) CaptureMethod.Manual else CaptureMethod.AutomaticAsync)
                .setDescription(description)
                .setMetadata(mapOf("viewtouch_check" to checkRef, "viewtouch_payment" to localRef))
            if (hold)
                builder.setPaymentMethodOptionsParameters(
                    PaymentMethodOptionsParameters.Builder()
                        .setCardPresentParameters(
                            CardPresentParameters.Builder().setRequestIncrementalAuthorizationSupport(true).build()
                        )
                        .build()
                )
            val params = builder.build()
            val status = Terminal.getInstance().offlineStatus
            val waiting = (status.reader ?: status.sdk).offlinePaymentAmountsByCurrency[currency.lowercase()] ?: 0L
            val behavior = if (hold || offlineMax <= 0 || amount > offlineMax || waiting + amount > offlineTotal)
                OfflineBehavior.REQUIRE_ONLINE else OfflineBehavior.PREFER_ONLINE
            current = null
            currentRef = localRef
            Terminal.getInstance().createPaymentIntent(params, object : PaymentIntentCallback {
                override fun onSuccess(paymentIntent: PaymentIntent) = collect(paymentIntent)
                override fun onFailure(e: TerminalException) = finish("error", e.errorMessage)
            }, CreateConfiguration(behavior))
        }
    }

    private fun collect(intent: PaymentIntent) {
        current = intent
        collecting = Terminal.getInstance().collectPaymentMethod(
            intent,
            object : PaymentIntentCallback {
                override fun onSuccess(paymentIntent: PaymentIntent) = confirm(paymentIntent)
                override fun onFailure(e: TerminalException) = failed(e)
            },
            // ViewTouch asks for the tip (the customer display); not again on the reader.
            CollectPaymentIntentConfiguration.Builder().skipTipping(true).build()
        )
    }

    // The card is read, not charged yet: a credit card pays the store's
    // surcharge (the reader shows it and asks the guest); debit, prepaid or
    // a card the reader can't tell, none.
    private fun confirm(intent: PaymentIntent) {
        collecting = null
        funding = intent.paymentMethod?.cardPresentDetails?.funding ?: ""
        val config = ConfirmPaymentIntentConfiguration.Builder()
        if (surchargeOffered > 0 && funding == "credit") {
            surchargePaid = surchargeOffered
            config.setSurcharge(
                SurchargeConfiguration.Builder(surchargeOffered)
                    .setConsent(SurchargeConsent.Builder(SurchargeConsentCollection.ENABLED).setNotice(surchargeNotice).build())
                    .build()
            )
        }
        Terminal.getInstance().confirmPaymentIntent(intent, object : PaymentIntentCallback {
            override fun onSuccess(paymentIntent: PaymentIntent) = approved(paymentIntent)
            override fun onFailure(e: TerminalException) = failed(e)
        }, config.build())
    }

    private fun approved(intent: PaymentIntent) {
        current = null
        // The card, from the charge (or the payment method, when the charge
        // isn't there; or what the reader kept, taken offline).
        val charged = intent.latestCharge?.paymentMethodDetails?.cardPresentDetails
        val presented = intent.paymentMethod?.cardPresentDetails
        val kept = intent.offlineDetails?.cardPresentDetails
        nativeResult(
            JSONObject()
                .put("status", "approved")
                .put("id", intent.id ?: "")
                // What was asked and the surcharge on top (however Stripe counts it in the amount).
                .put("amount", requested + surchargePaid)
                .put("funding", funding)
                .put("surcharge", surchargePaid)
                .put("brand", (charged?.brand ?: presented?.brand)?.toString() ?: kept?.brand ?: "")
                .put("last4", charged?.last4 ?: presented?.last4 ?: kept?.last4 ?: "")
                // Taken offline: Stripe gets it later (onPaymentIntentForwarded).
                .put("offline", intent.offlineDetails != null)
                .put("localRef", currentRef)
                .toString()
        )
    }

    private fun failed(e: TerminalException) {
        collecting = null
        val status = when {
            e.errorCode.name.startsWith("CANCELED") -> "canceled"
            e.paymentIntent?.status?.name == "REQUIRES_PAYMENT_METHOD" -> "declined"
            else -> "error"
        }
        // Nothing is left half-done on Stripe's side.
        current?.let { intent ->
            Terminal.getInstance().cancelPaymentIntent(intent, object : PaymentIntentCallback {
                override fun onSuccess(paymentIntent: PaymentIntent) {}
                override fun onFailure(e: TerminalException) {}
            })
        }
        current = null
        finish(status, e.errorMessage)
    }

    private fun finish(status: String, message: String) {
        nativeResult(JSONObject().put("status", status).put("message", message).toString())
    }

    // Stop waiting for the card (answers "canceled" through failed()).
    @JvmStatic
    fun cancel() {
        main.post {
            collecting?.cancel(object : Callback {
                override fun onSuccess() {}
                override fun onFailure(e: TerminalException) {}
            })
        }
    }
}
