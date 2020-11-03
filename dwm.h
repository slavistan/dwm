#pragma once

#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>


typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256]; /* window title */
	float mina, maxa; /* size aspects info */
	float cfact;
	int x, y, w, h; /* win geometry in pixels */
	int oldx, oldy, oldw, oldh; /* win geometry buffer (e.g. for fullscreen toggle) */
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw; /* border width */
	unsigned int tags; /* tag set (bit flags) */
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
	Client *next; /* next client in list */
	Client *snext; /* next client in focus stack */
	Client *swallowedby; /* client hidden behind me */
	Monitor *mon; /* monitor for this client */
	Window win; /* window id */
};

