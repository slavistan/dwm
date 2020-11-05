#!/usr/bin/env zsh

MYPATH=${0:A:h}

export DISPLAY=:4

startup() {
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
    "$MYPATH/../build/dwm" &
}

launchxephyr() {
    Xephyr -br -ac -noreset -resizeable :4 &
}

case "$1" in
    startup)
        startup
        ;;
    launchdwm)
        launchdwm
        ;;
    launchxephyr)
        launchxephyr
        ;;
esac