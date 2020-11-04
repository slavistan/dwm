#!/usr/bin/env zsh

cd ${0:A:h}

start_xephyr() {
  Xephyr -br -ac -noreset -resizeable :2 &
  sleep 1
}

build_and_launch() {

  make

  if ! pidof Xephyr; then
    printf "Xephyr not running.\n"
    exit 1
  fi

  sleep 1

  DISPLAY=:2 ./dwm
}

case "$1" in
  watch)
    start_xephyr
dwm.c
dwm.h
config.h
/home/stan/prj/dotfiles/scripts/startup/autostart.sh
EOF
    ;;
  @)
    shift
    "$@"
    ;;
  *)
    build_and_launch
    ;;
esac
