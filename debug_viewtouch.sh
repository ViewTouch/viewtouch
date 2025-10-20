#!/bin/bash

echo "ViewTouch Crash Debugging Script"
echo "================================"

# Find vt_main process
VT_MAIN_PID=$(ps aux | grep vt_main | grep -v grep | awk '{print $2}')

if [ -z "$VT_MAIN_PID" ]; then
    echo "Error: vt_main process not found. Make sure ViewTouch is running."
    echo "Start it with: sudo /usr/viewtouch/bin/vtpos"
    exit 1
fi

echo "Found vt_main process with PID: $VT_MAIN_PID"
echo "Attaching gdb..."

# Create gdb commands file
cat > /tmp/gdb_commands.txt << EOGDB
handle SIGPIPE nostop
handle SIGSEGV stop print
set pagination off
set logging file /tmp/viewtouch_crash_debug.log
set logging enabled on
continue
EOGDB

# Attach gdb to the process
sudo gdb -p $VT_MAIN_PID -x /tmp/gdb_commands.txt

echo ""
echo "Gdb session ended. Check /tmp/viewtouch_crash_debug.log for output."
echo "If a crash occurred, the log will contain the backtrace."
