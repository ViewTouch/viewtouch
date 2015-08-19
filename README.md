ViewTouch
=========
ViewTouch is the ORIGINAL Graphical Touchscreen Restaurant Point of Sale Interface, first created by Gene Mosher in 1986. https://commons.wikimedia.org/wiki/File:Comdex_1986.png

With the availability of ViewTouch source code under the GPL it is now possible for a restaurateur to automate a restaurant at minimal expense. ViewTouch allows Android tablets to be used as mobile or fixed POS terminals. The ViewTouch Android X Server is freely downloadable at http://viewtouch.com/download.html

_An important link at the top of this page is the Wiki link. It contains a page with build instructions._

Restaurateurs wishing training and 24/7 support need only contact support staff at http://www.viewtouch.com/contact.html

--Gene Mosher

Official website: http://www.viewtouch.com

License
=========

The ViewTouch code has recently been released under the GPLv3 license.

Commercial Support and Enquiries
=========
ViewTouch.com offers paid support to businesses and individuals want business class support for their Viewtouch Point Of Sale project. 

The official website and contact point for the ViewTouch company is at http://www.viewtouch.com/contact.html

Screenshots
=========
The following screenshots are in 1280 x 1024 resolution, however, the default graphical resolution is 1920 x 1080.

Time Clock and Secure Log On
![Touchscreen Password, Log On and Timeclock](http://www.viewtouch.com/vtscrn1.png)

Order Breakfast, then Display and/or Print in Kitchen
A Lightning Fast, High Resolution Interface
![Order Breakfast, then Display and/or Print in Kitchen](http://www.viewtouch.com/vtscrn6.png)

Time Clock Review and Edit - Control Labor Expense 
Back Office, Comprehensive Labor Costing, Overtime Alerts
Interactive Time Clock Review and Editing
![Time Clock Review and Edit - Control Labor Expense ](http://www.viewtouch.com/vtscrn3.png)

Decision Support: Fly-Over, Drill-Down in Real Time
Touch 'n' View Any Day or Any Period Updated Every Minute
![Decision Support: Fly-Over, Drill-Down in Real Time](http://www.viewtouch.com/vtscrn5.png)
ViewTouch doesn't just store all of your data for you - it keeps your entire transaction history in RAM. Rely on ViewTouch for the report data you need with perfect accuracy and lightning speed. Auditors can see compliance across every period. Control NON CASH revenue adjustments and labor costs, including non-intuitive details. The only way you can run a business is `by the numbers' and here are the numbers you need, Shift By Shift, Weekly, Monthly, Quarterly and Yearly.

History of the Viewtouch POS system
=========
ViewTouch first ran as a C program on the Atari ST computers. The Atari ST was a very exciting platform under Jack Tramiel from 1985 until 1993/4. In 1995 development of ViewTouch under UNIX began. At that time we were using Power Computing (i.e., Power PC) computers manufactured by The Computer Group at Motorola and the operating system was IBM's version of UNIX, which they called AIX.
In 1997, Steve Jobs returned to Apple, the first thing he did was to kill the clones and to do that he bought PowerComputing outright to stop sales cannibalization of Apple-branded models. That was the end of the PowerPC at Motorola.
When Atari died, if you didn't want to do Microsoft DOS or Apple, UNIX was your only choice, and X was your only choice to build the graphical interface. 
We moved from AIX on PowerPC to Red Hat on X86. In 2000 we began the transition to C++ and switched from Red Hat to Debian.  We remain with that as the default distribution today, featuring the XFCE and LXDE desktop environments.
Our preferred hardware platform is the Intel NUC with iCore Broadwell processors and M2.SATA with sustained sequential read rates up to 540 MB/s, Write up to 490 MB/s.  ARM also is a completely valid hardware platform for ViewTouch, notably on the RPi and Odroid computers. ViewTouch can easily run on thumbstick computers, too.

Discussion - On what hardware can I run this? Smaller ARM boards?
=========
Yes, absolutely, on the Raspberry Pi and the Odroid C1+. All you have to do is to target ARM when you compile. Jack Morrison has already done this, and so have at least three other people working on the code and/or selling ViewTouch where they live. Damon LoCascio in West London, John Watson in Edinburgh, Jack is in L.A., and others.
It isn't just the power of Linux; it's all about the unrealized power of the remote display capabilities of The X Window System. To add touchscreen terminals one doesn't need to bother copying the program and its data all across the network to add users - all one does is open another remote window to the application by adding its IP address to the ViewTouch GUI. It's that easy.

Payment gateway/processors
=========
The ViewTouch GUI has the Monetra Credit/Debit Card Verification engine integrated into it and can be used with every processor they allow, which is just about every one of them. If you visit the Monetra web site you can see the certifications, the site licenses they charge, which are quite reasonable, and the role they play in all this. ViewTouch does not have to play with any payment gateway at all - it's 100% optional.


ViewTouch POS on Android
=========

ViewTouch looks exactly the same on any Android tablet as it does on any display monitor, regardless of the resolution. The Android X Server we use is based on XSDL, which handles that transformation and much thanks to Sergii Pylypenko, of Kiev, for all of this!


