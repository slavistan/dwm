#!/usr/bin/env zsh

# Helful commands to control and set up dwm sessions inside xephyr

MYPATH=${0:A:h}

startup() {
  export DISPLAY=:4

  app1=gnome-mahjongg
  app2=evince

  $app1 >/dev/null 2>&1 &
  pid1=$!
  wid1=$(xdotool search --pid $pid1 --sync | head -1)
  $app2 >/dev/null 2>&1 &
  pid2=$!
  wid2=$(xdotool search --pid $pid2 --sync | head -1)
  printf "wid1= '$wid1', wid2= '$wid2'\n"
  zsh-xi st <<EOF &
wid1="$wid1"
wid2="$wid2"
echo "wid1=$wid1"
echo "wid2=$wid2"
EOF
}

launchdwm() {
  killdwm
  export DISPLAY=:4
  "$MYPATH/../build/dwm" &
}

killdwm() {
  pidof dwm | tr ' ' '\n' | while read pid; do
    display=$(cat /proc/$pid/environ | grep -z DISPLAY | tr -d '\0' | cut -d '=' -f2)
    if [ "$display" = ":4" ]; then
      # Immediately kill. Other signals trigger only once xephyr gets focus.
      # If this causes problems we may have to exit more gracefully.
      kill -9 $pid
    fi
  done
}

# Kill all apps running on display 4.
killapps() {
  export DISPLAY=:4
  xdotool search --onlyvisible --name ".*" | sort | uniq | while read wid; do
    if pid=$(xprop -id $wid | grep PID | tr -d ' ' | cut -d '=' -f2) && [ ! -z $pid ]; then
      echo "killing $pid"
      kill -TERM $pid
    fi
  done
}

launchxephyr() {
  Xephyr -br -ac -reset -resizeable :4 &
}

case "$1" in
  *)
    "$@"
    ;;
esac