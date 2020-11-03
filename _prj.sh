#!/usr/bin/env zsh

cd ${0:A:h}

build_and_launch() {
  set -e

  make

  # give xephyr time to start up
  Xephyr -br -ac -noreset -resizeable :1 &
  pid=$!
  export DISPLAY=":1"
  while ! ./dwm && kill -0 $pid; do
    sleep 0.1
  done
}

case "$1" in
  watch)
    entr -rc "$0" <<EOF
dwm.c
dwm.h
config.h
/home/stan/prj/dotfiles/scripts/startup/autostart.sh
EOF
    ;;
  *)
    build_and_launch
    ;;
esac
