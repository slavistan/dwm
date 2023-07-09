# dwm

**Forked from [suckless.org](http://dwm.suckless.org/).**

Notable modifications include:
- Bar and border transparency ([alpha patch](https://dwm.suckless.org/patches/alpha/))
- In-place restart of dwm ([restartsig patch](http://dwm.suckless.org/patches/restartsig/))
- Place new clients at bottom of stack ([attachbottom patch](http://dwm.suckless.org/patches/attachbottom/))
- Execute script at startup ([autostart patch](http://dwm.suckless.org/patches/autostart/))
- Move clients across the stack ([movestack patch](https://dwm.suckless.org/patches/movestack/))
- Set a client's relative size within slave area ([cfact patch](https://dwm.suckless.org/patches/cfacts/))
- Activate client on NET_ACTIVE_WINDOW client messages ([focusonnetactive patch](https://dwm.suckless.org/patches/focusonnetactive/))
  + Allows to activate clients by their window ID using tools like xdotool
- Dynamic window swallowing ([dynamicswallow patch](https://dwm.suckless.org/patches/dynamicswallow/))

# Setup

Build instructions:

```
mkdir build
cd build
cmake ..
make
```

- TODO: Cleanup and reference xinerama scripts
- TODO: Add directory listing

# Todos & Roadmap

- FIXME: Changing active tags sometimes forgets focus history and activates wrong window
- FIXME: dmenu is not always shown on the currently focused monitor. When there are no clients on the second monitor, dmenu will appear on the wrong screen.
- TODO: Implement duplex communication via unix-sockets (+ CLI tool) for full scriptability.
- TODO: Add functionality to fix the focus to a specific window. This is useful to keep the focus on an editor window while writing or debugging scripts/code which spawn windows themselves. Alternatively, the functionality may simply disable the autofocus of new windows.
