#!/bin/sh

killall vt_main vt_term
rm /usr/viewtouch/bin/.vt_is_running
exec /usr/viewtouch/bin/vtpos

