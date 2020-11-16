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

_launch_setup() {
	xrandr --listmonitors 2>/dev/null | tail -n +2 |
		cut -d ' ' -f 3 | grep -v '^+' | while read line; do
		xrandr --delmonitor "$line" 2>/dev/null
	done

	geom=$(xrandr 2>/dev/null | head -2 | tail -n +2 | cut -d ' ' -f 3 |
		tr 'x+' ' ')
	w=$(echo "$geom" | cut -d ' ' -f 1)
	h=$(echo "$geom" | cut -d ' ' -f 2)
	w1=$((w / 2))
	w2=$((w - w1))

	xrandr --setmonitor default-1 $w1/0x$h/0+0+0 default >/dev/null 2>&1
	xrandr --setmonitor default-2 $w2/0x$h/0+$w1+0 none >/dev/null 2>&1
	feh --bg-scale ~/dat/img/wall
}

_ls() {
	export DISPLAY=:4
	xdotool search --maxdepth 1 --name ".*" | while read w; do
		name=$(xprop  -id $w | grep "WM_CLASS" | awk -F '= ' '{ print $2 }')
		printf "$w\t'$name'\n"
	done
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
@)
	shift
	"$@"
	;;
*)
	"$@"
	;;
esac

sleep 0.1 # vscode unfuck for background processes
