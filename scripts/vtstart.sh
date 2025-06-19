#!/bin/sh

killall vt_main vt_term
rm /usr/viewtouch/bin/.vt_is_running
export DISPLAY=:0 # Fix: Ensure X11 display is set before launching vtpos
exec /usr/viewtouch/bin/vtpos

