# Shrinking the Java side of the app (R8). Kept whole: Qt's classes and
# ViewTouch's own (C++ calls them by name through JNI), and Stripe
# Terminal's (reflection and generated protocol classes; its card path
# can't be tried without a reader). What they don't use of the libraries
# they bring (Kotlin, ktor, wire, moshi, gson) goes. Names stay as they are.
-dontobfuscate
-keep class org.qtproject.** { *; }
-keep class org.viewtouch.** { *; }
-keep class com.stripe.** { *; }
-keepattributes *Annotation*,Signature,InnerClasses,EnclosingMethod
-dontwarn **
