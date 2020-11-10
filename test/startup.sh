#!/usr/bin/env zsh

# Helful commands to control and set up dwm sessions inside xephyr

MYPATH=${0:A:h}

errln() { echo "\033[31;1m[ERR ]\033[0m $@"; }
logln() { echo "\033[32;1m[INFO]\033[0m $@"; }
die() { echo "$@"; exit 1; }

_test_swallowfloating() {
  zsh-xi st -n st-float-dingo <<"EOF" &
wid=$(xdotool search --classname st-float-dingo)
echo "I am window $wid"
xsetroot -name ::swallownext:$wid
EOF
	xdotool search --classname st-float-dingo --sync
	sleep 1
	zathura &
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
  test)
    shift
    [ ! $# -eq 1 ] && die "Usage: $0 test <NAME>"
	export DISPLAY=:4
    _test_$1
    ;;
  *)
    "$@"
    ;;
esac