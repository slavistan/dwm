#!/usr/bin/env zsh

cd ${0:A:h}

start_xephyr() {
  Xephyr -br -ac -reset -terminate -resizeable :4 &
  sleep 1
}

build_and_launch() {

  make
  start_xephyr
  DISPLAY=:4 ./dwm
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
  @)
    shift
    "$@"
    ;;
  *)
    build_and_launch
    ;;
esac
