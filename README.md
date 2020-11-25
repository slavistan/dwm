# dwm

**Unpatched original found at [suckless.org](http://dwm.suckless.org/).**

Notable modifications include:
  - Bar and border transparency ([alpha patch](https://dwm.suckless.org/patches/alpha/)]
  - In-place restart of dwm ([restartsig patch](http://dwm.suckless.org/patches/restartsig/))
  - Place new clients at bottom of stack ([attachbottom patch](http://dwm.suckless.org/patches/attachbottom/))
  - Execute script at startup ([autostart patch](http://dwm.suckless.org/patches/autostart/))
  - Move clients across the stack ([movestack patch](https://dwm.suckless.org/patches/movestack/))
  - Set a client's relative size within slave area ([cfact patch](https://dwm.suckless.org/patches/cfacts/))
  - Activate client on NET_ACTIVE_WINDOW client messages (([focusonnetactive patch](https://dwm.suckless.org/patches/focusonnetactive/))
    + Allows to activate clients by their window ID using tools like xdotool

# Todos

TODO(fix): Changing active tags sometimes forgets focus history.
  - Wrong window is activated
TODO: Include relevant portion of dwm's original README (e.g. license)
TODO: Build instructions