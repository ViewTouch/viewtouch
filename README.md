Viewtouch
=========
ViewTouch is the world renowned, ORIGINAL Graphical Touchscreen Restaurant Point of Sale Interface.  Every other POS solution in the world came after we first created ViewTouch in 1986 and demonstrated it to many thousands worldwide.

ViewTouch is the ONLY POS solution which encourages you to buy and attach as many of your own Android tablets to the POS system as you want without paying us anything!  Buy as many Android tablets of any size, any resolution as you want, and add them to your POS system.


With the availability of ViewTouch source code under the GPL and of the Raspberry Pi computer it is now possible for a restaurateur to automate a restaurant of small to moderate size for about an hour's worth of sales revenue, or for a single day's profit. ViewTouch allows multiple touchscreen terminals, each powered by a Raspberry Pi. ViewTouch also allows Android tablets to be used as mobile POS terminals, making use of the ViewTouch X Server apk, downloadable from the ViewTouch web site.

Restaurateurs wishing training and 24/7 support need only contact the support staff at http://www.viewtouch.com/contact.html

Running ViewTouch requires graphical data files which are available at http://www.viewtouch.com/download.html
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
ViewTouch doesn't just store all of your data for you - it keeps your entire transaction history in Main Memory.  No Wonder It's Fast !
Every restaurant needs these numbers. Rely on ViewTouch for the numbers you need with perfect accuracy and lightning speed. No one will ever be able to find fault with record keeping like this; auditors can see compliance across every period. Control NON CASH revenue adjustments and labor costs, including non-intuitive details. The only way you can run a business is `by the numbers' and here are the numbers you need, Shift By Shift, Weekly, Monthly, Quarterly and Yearly.

History of the Viewtouch POS system
=========
ViewTouch first ran as a C program on the Atari ST computers. The Atari ST was a very exciting platform under Jack Tramiel from 1985 until 1993/4. In 1995 development of ViewTouch under UNIX began. At that time we were using Power Computing (i.e., Power PC) computers manufactured by The Computer Group at Motorola and the operating system was IBM's version of UNIX, which they called AIX.
In 1997, Steve Jobs returned to Apple, the first thing he did was to kill the clones and to do that he bought PowerComputing outright to stop sales cannibalization of Apple-branded models. That was the end of the PowerPC at Motorola.
When Atari died, if you didn't want to do Microsoft DOS or Apple, UNIX was your only choice, and X was your only choice to build the graphical interface. 
We moved from AIX on PowerPC to Red Hat on X86. In 2000 we began the transition to C++ and switched from Red Hat to Debian.  We remain with that as the default distribution today, featuring the XFCE and LXDE desktop environments. ARM also is a completely valid hardware platform for ViewTouch, notably on the RPi and Oduino computers. ViewTouch can easily run on thumbstick computers, too.

Discussion - On what hardware can I run this? Smaller ARM boards?
=========
Yes, absolutely, and even on the Oduino C1, which is also $35, but which runs faster than the latest RPi. All you have to do is to target ARM. Jack Morrison has already done this, and so have at least three other people working on the code and/or selling ViewTouch where they live. Damon LoCascio in West London, John Watson in Edinburgh, Jack is in L.A., and others.
$35 Point of Sale to match and surpass the performance of $35,000 POS offerings from many companies is what this is all about, and nothing less than that. It isn't just the power of Linux; it's all about the unrealized power of the remote display capabilities of The X Window System. One doesn't need to bother copying the program and its data all across the network to add users - all one does is open another remote window to the application by adding its IP address to the ViewTouch GUI. It's that easy.

Payment gateway/processors
=========
The ViewTouch GUI has the Monetra Credit/Debit Card Verification engine integrated into it and can be used with every processor they allow, which is just about every one of them. If you visit the Monetra web site you can see the certifications, the site licenses they charge, which are quite reasonable, and the role they play in all this. ViewTouch does not have to play with any payment gateway at all - it's 100% optional.


ViewTouch POS GUI comments
=========
The interface ideas were first implemented 30 years ago, and the ideas were first in my head nearly 40 years ago. And if having software developed according to a singular but evolving vision for decades is a good idea, then this is certainly that. In the 70's and 80's there was a desperate need to figure out how to put computers to use in small businesses, especially restaurants, in a way that would work for everyone who worked there, and would work for the customers, too. It definitely required uninventing the cash register and replacing it with 'something'. That was the problem I faced, which we all faced, and I did my best to solve it, even though I had no experience, no credentials, and no support from any company in either the computer business or in the hospitality business. And here I am, 30 years later, at this point, looking at what comes next, instead of retiring, like almost everyone else in my life. There's nothing else I can even imagine doing than this.

Webpage design comments
========
I spend virtually all of my time and money improving the ViewTouch software; I've spent very little time and no money at all in the web site. I figure that if the software that was written 20 years ago and works great then a web site that was written 20 years ago should work well enough. The software is heavily influenced by the work done by Richard W. Stevens, who passed away in 1999. Younger people are fond of telling me how outdated the ViewTouch web site is but older people typically tell me how nice it is to run across a web site that just lets them read about ViewTouch in as much depth as they care to. My favorite car and my favorite music are 50 years old. The house I grew up in is 250 years old and the castle I live in when I am in Belgium visiting my classmates from 50 years ago is 900 hundred years old. I like things that are aged.  If they're still around then it's for good reason.

ViewTouch POS on Android
=========

ViewTouch looks exactly the same on any Android tablet as it does on any display monitor, regardless of the resolution. The Android X Server we use is based on XSDL, which handles that transformation and much thanks to Sergii Pylypenko, of Kiev, for all of this!

Discussion and comments regarding changing license to GPLv3
=========
Since putting the source under GPL and hosting it at GitHub in November I and other ViewTouch associates have paid programmers several thousand dollars to update, enhance, debug and extend the code. My hope is that many will use it to automate at very low, nominal cost, the operation of their restaurants and, now, event venues, thus avoiding the excessive and often prohibitive costs of automation and POS software which is available from hundreds of companies which have copied the graphical touchscreen user interface ideas which I first developed and popularized in 1985-86.
The precise bottom line is that these companies have freely copied and profited from use of my copyrighted interface without any payment or acknowledgment. I declare this free ride for them to be over and that the free ride for the public has begun. I therefore offer and invite people throughout world to freely use the source code and graphic programming framework I have been developing for 30 years, thus freeing themselves from having to buy point of sale and automation software for restaurants and event venues from any company looking to sell the POS software they have written and are selling at very inflated prices.
Larry Ellison, of Oracle, for example, recently paid $5.4 billion for the Micros POS company and its POS systems, while very popular, are also very expensive and available only from Oracle's Micros dealerships. NCR (formerly National Cash Register), in another example, recently paid $1.25 billion for the Radiant POS company and its Aloha POS systems, which are also very popular and very expensive, and only available from NCR. Toshiba recently bought the IBM POS business in a similar deal. I will match ViewTouch, feature for feature, against any POS software in the world, and will provide a superior user interface to any which can be customized by the integrated drag & drop authoring environment which has been the hallmark of the ViewTouch interface for 30 years.
ViewTouch runs nicely on the Raspberry Pi, and any x86, ARM or MIPS platform which also runs X. It is therefor possible to install POS in a restaurant or a theater for the price of a Raspberry Pi or ODroid-C1, for example, each of which sells for about $45 with case and power supply. ViewTouch runs on any resolution and comes with an X Server for Android, also available on GitHub, so that any number of displays and/or tablets can be used simultaneously, even over the wide area network. I have customers around the world; ViewTouch is proven.

One industry in which software licensed under the GPL is NOT doing very well is the hospitality industry. With the goal of changing this, about 6 months ago I placed my restaurant point of sale software under the GPL. With the liberty that this change makes possible I encourage anyone who is interested in hospitality software available under the GPL to take a look at ViewTouch. It's available at GitHub and it is a point of sale solution for restaurants which makes full use of the remote display capabilities of the X Window System, of the X Server for Android and of the amazing low prices for hardware these days, especially the Raspberry Pi. Restaurants, Bars and Clubs can be automated at virtually no cost these days, using GPL licensed software.
