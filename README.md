<div align="center">

# ViewTouch©

**The Original Graphical Touchscreen Restaurant Point of Sale Interface**

[![Build Status](https://img.shields.io/travis/ViewTouch/viewtouch/master.svg?label=Travis)](https://travis-ci.org/ViewTouch/viewtouch/builds)
[![Discord](https://img.shields.io/discord/YOUR_SERVER_ID?color=7289da&label=Discord&logo=discord&logoColor=white)](https://discord.com/invite/ySmH2U2Mzb)
[![Join the chat at https://gitter.im/ViewTouch/viewtouch](https://badges.gitter.im/ViewTouch/viewtouch.svg)](https://gitter.im/ViewTouch/viewtouch?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

*ViewTouch is a registered trademark in the USA*

[Website](http://www.viewtouch.com) • [Wiki & Documentation](../../wiki) • [Download Image](http://www.viewtouch.com/nc.html)

</div>

---

## Table of Contents

- [About](#about)
- [Quick Start](#quick-start)
- [Hardware](#hardware)
- [Screenshots](#screenshots)
- [History](#history)
- [Contact & Support](#contact--support)
- [License](#license)

---

## About

ViewTouch is a powerful, open-source Point of Sale system designed specifically for the restaurant and hospitality industry. With over four decades of development, it combines the reliability of Linux with the flexibility of the X Window System to deliver a robust, network-transparent POS solution.

### Key Features

- **Lightning-fast performance** - Entire transaction history kept in RAM for instant reporting
- **Network transparency** - Remote display capability via X Window System
- **Comprehensive reporting** - Real-time drill-down analytics and decision support
- **Labor management** - Interactive time clock with overtime alerts and cost control
- **Cross-platform** - Runs on Raspberry Pi 4/5, x86, and ARM architectures
- **Mobile integration** - Android tableside order entry support
- **Multilingual support** - Built-in language pack system
- **Cross platform solution** - Can be used on a wide variaty of linux distros thanks to the universal installer

> **Note:** For build instructions and the latest announcements, visit the [Wiki](../../wiki).

---

## Quick Start

### Download Pre-built Image

[Download the latest ViewTouch image](http://www.viewtouch.com/nc.html) for Raspberry Pi (4 or 5). Write the image to a 32GB or larger microSD card, and boot directly to the ViewTouch desktop with full POS functionality.

#### Building

See the [Wiki](../../wiki) for detailed build instructions and development setup.

---

## Hardware

### Official ViewTouch Point of Sale Computer

<div align="center">

![ViewTouch Point of Sale Computer with 15.8" Display](https://www.viewtouch.com/poscomputer.avif)

**15.8" All-in-One Touchscreen POS Computer**  
Powered by Raspberry Pi Compute Module 5  
Manufactured by Chipsee, Beijing, China

</div>

ViewTouch offers all-in-one Point of Sale computers featuring:
- 15.8" capacitive touchscreen display (1920×1080 default resolution)
- Raspberry Pi 5 Compute Module
- Pre-installed ViewTouch software
- Network-ready for multi-terminal setups

With open-source code and Raspberry Pi hardware, restaurateurs can now fully automate their operations with complete control over their POS system, including source code access.

### Mobile Support

Thanks to Pelya (Sergii Pylypenko) and Ariel Brambila Pelayo for ensuring that with [Xsdl](https://apkpure.com/xserver-xsdl/x.org.server) mobile Android devices as wireless ViewTouch tableside order entry displays.

---

## Screenshots

> *Screenshots shown in 1280×1024 resolution. Default resolution is 1920×1080.*

### Time Clock and Secure Log On

![Touchscreen Password, Log On and Timeclock](http://www.viewtouch.com/vtscrn1.png)

---

### Order Entry - Lightning Fast Interface

**Order Breakfast, then Display and/or Print in Kitchen**

![Order Breakfast, then Display and/or Print in Kitchen](http://www.viewtouch.com/vtscrn6.png)

---

### Labor Management

**Time Clock Review and Edit - Control Labor Expense**  
Back Office, Comprehensive Labor Costing, Overtime Alerts, Interactive Time Clock Review and Editing

![Time Clock Review and Edit - Control Labor Expense](http://www.viewtouch.com/vtscrn3.png)

---

### Real-time Analytics

**Decision Support: Fly-Over, Drill-Down in Real Time**  
Touch 'n' View Any Day or Any Period Updated Every Minute

![Decision Support: Fly-Over, Drill-Down in Real Time](http://www.viewtouch.com/vtscrn5.png)

ViewTouch doesn't just store your data—it keeps your entire transaction history in RAM for perfect accuracy and lightning speed. Auditors can see compliance across every period. Control non-cash revenue adjustments and labor costs, including non-intuitive details, shift by shift, weekly, monthly, quarterly, and yearly.

---

## History

### Origins (1979-1986)

Gene Mosher began writing Point of Sale code on an Apple ][ in 1979. ViewTouch© as we know it today was first created by restaurateur Gene Mosher and C programmer Nick Colley in **1986**, making it the **ORIGINAL Graphical Touchscreen Restaurant Point of Sale Interface**.

**First Public Demonstration:** ComDex, Las Vegas, November 1986  
[See historical photo](https://commons.wikimedia.org/wiki/File:Comdex_1986.png)

**Early Platform:** Atari ST computers (1986-1993/4 under Jack Tramiel)

**Initial Funding & Support:**
- Ed Ramsay (1986 - code development funding)
- Barbara Mosher (1986 - present, ongoing financial support)
- John King, M.D., Chicago (early funding)

### Modern Era (1995-1998)

The current version of ViewTouch was created by Gene Mosher and C programmer Richard Bradley, transitioning to:
- **UNIX (AIX)** on Motorola PowerPC (1995)
- **MIT's X Window System** for network-transparent graphical interface (1995)
- **Linux on Intel x86** (1997)

**Major Funding:** Billy Foster (1995-1998)

### Enhancement Period (2000-2004)

- **C to C++ transition** (2000-present day)
- Extensive code enhancement by Bruce King (2000-2004) Ariel Brambila Pelayo (2025)
- **Major Funding:** Doug DeLeeuw

### Open Source Era (2014-Present)

ViewTouch released under GNU Public License (GPL v3) with code hosted on GitHub. Significant modernization and refinement by the open-source community:

**Key Contributors:**
- **Jack Morrison** - Amazing debugging skills
- **NeroBurner (Reinhold Gschweicher)** - Major refactoring and standardization
- **NoOne558 (Ariel Brambila Pelayo)** - Upgraded to Xft scalable fonts, mobile integration, AI-empowered refactoring of the entire ViewTouch code base.
- **Gene and Barbara Mosher** - Lifetime support and funding
- **Ariel Brambila Pelayo** - awarded co-ownership status of ViewTouch code and documentation at GitHub

### Platform Evolution

| Year | Platform | Key Technology |
|------|----------|----------------|
| 1979 | Apple ][ | Initial POS code |
| 1986 | Atari ST | First GUI touchscreen POS |
| 1995 | AIX/PowerPC | UNIX + X Window System |
| 1997 | Linux/x86 | Intel architecture |
| 2000 | Linux/x86 | C++ transition |
| 2016+ | Raspberry Pi | Default platform (Pi 4/5) |

**Current Stack:**
- **OS:** Debian Linux with XFCE desktop
- **Display:** X Window System (network-transparent)
- **Default Hardware:** Raspberry Pi family
- **Auto-update:** Desktop icon compiles and installs latest code from GitHub

Gene Mosher's passion and vision has overseen the development, management, and maintenance of ViewTouch code across six decades, spanning a wide array of computers and point of sale equipment. Gene owns the ViewTouch copyright and remains active in ViewTouch in 2025.  A personal note to everyone from Gene: I am getting on in years and everyone has always wanted me to tell them what will happen to ViewTouch if/when something happens to me. In 2025 I can reassure that I will be participating in the development of ViewTouch until my last breath, which could easily be 2050 (I'll be 101 then), and with Ariel, who is about half a century younger than I am, being awarded co-ownership of the ViewTouch code and documentation at ViewTouch/GitHub, nothing bad can happen, is going to happen, to ViewTouch.  I myself used to wonder about the future of ViewTouch, but I no longer do wonder about the future of ViewTouch.  It's going to be alive, healthy, vibrant and dynamic for a long, long time.  A new chapter has been opened for ViewTouch in 2025 and I could not be happier about it.  Anyone can be a part of the future of ViewTouch - I welcome all who will join us.

---

## Contact & Support

### Commercial Support

**ViewTouch Official**
- **Website:** [www.viewtouch.com](http://www.viewtouch.com)
- **Email:** gene@viewtouch.com
- **Phone:** 541-515-5913

### Community

- **Discord:** [Join our server](https://discord.com/invite/ySmH2U2Mzb)
- **GitHub Issues:** Bug reports and feature requests
- **Gitter Chat:** [Join the conversation](https://gitter.im/ViewTouch/viewtouch)
- **Wiki:** Documentation and guides

The availability of ViewTouch source code and documentation at GitHub benefits clients, customers, and associates of Gene Mosher, facilitating the enhancement and extension of ViewTouch source code.

---

## License

ViewTouch is released under the **GNU General Public License, version 3 (GPL v3)**.

See [LICENSE](LICENSE) for full details.

---

## Payment Processing

ViewTouch does not manage electronic payment processing. Please make your own decisions with regard to Electronic Funds Transfer functionality.

---

<div align="center">

**ViewTouch** - *Six decades of innovation in restaurant technology*

Made with ❤️ by the ViewTouch community

</div>
