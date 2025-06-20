#!/bin/sh

# --- Script changes summary ---
# 1. Only update vt_data if missing, smaller than 60KB, or not a gzip file
# 2. Set DISPLAY=:0 before launching vtpos to fix X11 issues
# 3. Kill old vt_main and vt_term processes before starting
# -----------------------------

killall vt_main vt_term
rm /usr/viewtouch/bin/.vt_is_running
# Fix: Only update vt_data if missing, smaller than 60KB, or not a gzip file
if [ ! -f /usr/viewtouch/bin/vt_data ] || [ $(stat -c%s /usr/viewtouch/bin/vt_data) -lt 61440 ] || ! file /usr/viewtouch/bin/vt_data | grep -q 'gzip compressed data'; then
    /usr/viewtouch/bin/vtdata.sh
fi
export DISPLAY=:0 # Fix: Ensure X11 display is set before launching vtpos
exec /usr/viewtouch/bin/vtpos

