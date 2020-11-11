#!/usr/bin/env zsh

# Helful commands to control and set up dwm sessions inside xephyr

MYPATH=${0:A:h}

errln() { echo "\033[31;1m[ERR ]\033[0m $@"; }
logln() { echo "\033[32;1m[INFO]\033[0m $@"; }
die() { echo "$@"; exit 1; }

_test_swallow_tiling() {
	zsh-xi st -n bingo <<"EOF" &
wid=$(xdotool search --classname bingo)
echo "I am window $wid"
xsetroot -name ::swallownext:$wid
EOF
	xdotool search --classname bingo --sync
	sleep 1
	zathura &
}

_test_swallow_floating() {
	zsh-xi st -n st-float-dingo <<"EOF" &
wid=$(xdotool search --classname st-float-dingo)
echo "I am window $wid"
xsetroot -name ::swallownext:$wid
EOF
	xdotool search --classname st-float-dingo --sync
	sleep 1
	zathura &
}

_launch_dwm() {
	killdwm
	export DISPLAY=:4
	"$MYPATH/../build/dwm" &
}

_kill_dwm() {
	pidof dwm | tr ' ' '\n' | while read pid; do
		display=$(cat /proc/$pid/environ | grep -z DISPLAY | tr -d '\0' | cut -d '=' -f2)
		if [ "$display" = ":4" ]; then
			# Immediately kill. Other signals trigger only once xephyr gets focus.
			# If this causes problems we may have to exit more gracefully.
			kill -9 $pid
		fi
	done
}

# Kill all clients (except dwmbar & rootwin)
_kill_clients() {
	rootwid=$(xdotool search --maxdepth 0 --name '.*')
	xdotool search --maxdepth 1 --name '.+' | grep -v "$rootwid" | while read wid; do
		xdotool windowkill $wid
	done
	return 0 # always return success
}

_launch_xephyr() {
	DISPLAY=:0 Xephyr -br -ac -reset -resizeable :4 &
}


export DISPLAY=:4
case "$1" in
test)
	shift
	[ $# -eq 0 ] && die "Usage: $0 test WHAT..."
	"_test_${*:gs/ /_}"
	;;
launch)
	shift
	[ ! $# -eq 1 ] && die "Usage: $0 launch WHAT"
	_launch_$1
	;;
kill)
	shift
	[ ! $# -eq 1 ] && die "Usage: $0 kill WHAT"
	_kill_$1
	;;
*)
	"$@"
	;;
esac

sleep 0.1 # vscode unfuck for background processes