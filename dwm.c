/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
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

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)

#define OPAQUE                  0xffU

#ifndef VERSION
#define VERSION "undefined"
#endif

/* enums */
enum { CurNormal, CurResize, CurMove, CurSwal, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeStatus }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */
enum { ClientRegular = 1, ClientSwallowee, ClientSwallower }; /* client types wrt. swallowing */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256]; /* window title */
	float mina, maxa; /* size aspects info */
	float cfact; /* relative size in slave area */
	int x, y, w, h; /* win geometry in pixels */
	int oldx, oldy, oldw, oldh; /* win geometry buffer (e.g. for fullscreen toggle) */
	int basew, baseh, incw, inch, maxw, maxh, minw, minh; /* size hints */
	int bw, oldbw; /* border width */
	unsigned int tags; /* tag set (bit flags) */
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
	Client *next; /* next client in list */
	Client *snext; /* next client in focus stack */
	Client *swallowedby; /* client hidden behind me */
	Monitor *mon; /* monitor for this client */
	Window win; /* window id */
};

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

struct Monitor {
	char ltsymbol[16]; /* layout symbol string show in bar */
	float mfact; /* relative size of master area [0, 1] */
	int nmaster; /* num of windows in master area (relevant for tiling layout) */
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area; draw area for clients */
	int gappx;            /* gaps between windows */
	unsigned int seltags; /* selected tags */
	unsigned int sellt;   /* selected layout (index into Monitor.lt) */
	unsigned int tagset[2];
	int showbar;
	int topbar;
	Client *clients; /* client list */
	Client *sel;     /* active client */
	Client *stack;   /* focus stack */
	Monitor *next;
	Window barwin; /* Window handle of the monitor's status bar */
	const Layout *lt[2];
};

typedef struct {
	const char *class;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int monitor;
} Rule;

typedef struct Swallow Swallow;
struct Swallow {
	/* Window class name, instance name (WM_CLASS) and title
	 * (WM_NAME/_NET_WM_NAME, latter preferred if it exists). An empty string
	 * implies a wildcard as per strstr(). */
	char class[256];
	char inst[256];
	char title[256];
	Client *client; /* swallower */
	Swallow *next;
};

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void attachbottom(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static int fakesignal(void);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void manageswallow(Client *c, Window w);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static void moveclient(const Arg *arg);
static Client *nexttiled(Client *c);
static void pop(Client *);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void restack(Monitor *m);
static void run(void);
static void runstartup(void);
static void scan(void);
static int sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setgaps(const Arg *arg);
static void setlayout(const Arg *arg);
static void setcfact(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void sigchld(int unused);
static void sighup(int unused);
static void sigterm(int unused);
static void spawn(const Arg *arg);
static void swal(Client *swer, Client *swee);
static void swalqueue(Client *c, const char* class, const char* inst, const char* title);
static void swalmouse(const Arg *arg);
static void swalstop(Client *c);
static void swalstopsel(const Arg *arg);
static void swalunqueue(Swallow *s);
static void statusclick(const Arg *arg);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static int wintoclient2(Window w, Client **pc, Client **proot);
static Swallow *wintoswallow(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void xinitvisual();
static void zoom(const Arg *arg);

/* variables */
static const char broken[] = "broken"; /* name for broken client which do not set WM_CLASS*/
static char stext[256];      /* status text */
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh, blw = 0;      /* bar geometry: blw = bar layout segment width */
static int lrpad;            /* sum of left and right padding for tag text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	// How are CirculateRequest handled?
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static int restart = 0;
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon; /* monitor list, selected monitor */
static Window root, wmcheckwin;
static Swallow *swallows; /* list of registered swallows */

static int useargb = 0;
static Visual *visual;
static int depth;
static Colormap cmap;

/* configuration, allows nested code to access above variables */
#include "config.h"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */

/*
 * Applies per-window rules defined in config.h
 */
void
applyrules(Client *c)
{
	const char *class, *instance;
	unsigned int i;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->isfloating = 0;
	c->tags = 0;
	XGetClassHint(dpy, c->win, &ch);

	/* Label clients which don't define a class or instance name as broken. */
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			c->isfloating = r->isfloating;
			c->tags |= r->tags;
			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);

 	/* interact is set when moving or resizing using the mouse */
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}

	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;

	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}

	// Does the return value indicate whether a resize is necessary?
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

/*
 * TODO: What does arrange() do?
 */
void
arrange(Monitor *m)
{
	if (m) {
		showhide(m->stack);
	}
	else {
		for (m = mons; m; m = m->next) {
			showhide(m->stack);
		}
	}

	if (m) {
		arrangemon(m);
		restack(m);
	}
	else {
		for (m = mons; m; m = m->next)
			arrangemon(m);
	}
}

/*
 * Call a monitor's arrange() function
 */
void
arrangemon(Monitor *m)
{
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

/*
 * Attach client at the front of its monitor's client list.
 * Assumes c->mon is set.
 */
void
attach(Client *c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

/*
 * Attach client to end of its monitor's client list.
 * Assumes c->mon is set.
 */
void
attachbottom(Client *c)
{
	Client *below;

	for (below = c->mon->clients; below && below->next; below = below->next);
	c->next = NULL;
	if (below)
		below->next = c;
	else
		c->mon->clients = c;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
buttonpress(XEvent *e)
{
	/* In addition to clicks on the rootwin, the bars and unfocused top-level
	 * windows, buttonpress events for clicks on clients are received for all
	 * button/modifier pairs as defined in setup()'s call to grabbuttons(). */

	unsigned int i, x, click;
	int at, cindex;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}

	if (ev->window == selmon->barwin) {
		/* Determine where the bar was clicked. */

		i = x = 0;
		do
			x += TEXTW(tags[i]);
		while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} else if (ev->x < x + blw) {
			click = ClkLtSymbol;
		}
		else if ((at = ev->x - (selmon->ww - TEXTW(stext) + lrpad - statusrpad)) >= -lrpad/2) {
			if (at >= 0 && (cindex = drw_fontset_utf8indexat(drw, stext, at)) >= 0) {
				click = ClkStatusText;
				arg.ui = cindex;
			}
			else {
				click = ClkWinTitle; /* close vanity gap left of status */
			}
		}
		else {
			click = ClkWinTitle;
		}
	} else if ((c = wintoclient(ev->window))) {
		focus(c);
		restack(selmon);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}

	const Arg *parg;
	for (i = 0; i < LENGTH(buttons); i++) {
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state)) {
			switch (click) {
			case ClkStatusText:
				/* encode button into the 3 most sig. bits */
				arg.ui |= (buttons[i].button << (sizeof(unsigned) * CHAR_BIT - 3));
				parg = &arg;
				break;
			case ClkTagBar:
				parg = buttons[i].arg.i == 0 ? &arg : &buttons[i].arg;
				break;
			default:
				parg = &buttons[i].arg;
				break;
			}
			buttons[i].func(parg);
		}
	}
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (mons)
		cleanupmon(mons);
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(colors); i++)
		free(scheme[i]);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);
	free(mon);
}

void
clientmessage(XEvent *e)
{
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);
  unsigned int i;

	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		for (i = 0; i < LENGTH(tags) && !((1 << i) & c->tags); i++);
		if (i < LENGTH(tags)) {
			const Arg a = {.ui = 1 << i};
			view(&a);
			focus(c);
			restack(selmon);
		}
	}
}

/*
 * Inform client window about it's (new) geometry via synthetic ConfigureNotify
 */
void
configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce); // async, right?
}

void
configurenotify(XEvent *e)
{
	Monitor *m;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		/* Adjust size of windows if root's size changes */
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen)
						resizeclient(c, m->mx, m->my, m->mw, m->mh);
				XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *e)
{
	/* XPM 2.2.1: A window's configuration consists of its position, extent,
	 * border width and stacking order. These window parameters are special
	 * in that they are configured in cooperation with the window manager. */
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = wintoclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth) {
			// ?? What makes changing the border width exclusive? Why are
			// possible other changes ignored if bw is set? Also, does this
			// ever do anything to X?
			c->bw = ev->border_width;
		}
		else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			/* If either the window is floating or the current layout does not come
			 * with an arrange function update the client's geometry fields. */
			m = c->mon;
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->my + ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}

			/* Keep the window within the confines of the monitor. Snaps back to center. */
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */

			/* ICCCM 4.1.5: If moving a client without resizing it or changing
			 * its border width, the client shall receive a synthetic
			 * ConfigureNotify event that describes the new geometry of the
			 * window. */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);

			/* Reconfigure only such windows which are in the selected tagset.
			 * When changing the tag everything will be redrawn anyway. */
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		}
		else {
			/* Refuse any move/resize requests for tiled windows. This refusal
			 * is communicated to the client via a synthetic ConfigureNotify
			 * event that describes the unchanged geometry of the window. See
			 * ICCCM 4.1.5. */
			configure(c);
		}
	}
	else {
		/* No client exists which corresponds to the event's window, in which
		 * case forward the request verbatim to X. Presumably, this is only the
		 * case for unmapped windows (whose map request will create a
		 * corresponding client via manage()).  */
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

Monitor *
createmon(void)
{
	Monitor *m;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = 1;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->topbar = topbar;
	m->gappx = gappx;
	m->lt[0] = &layouts[0];
	m->lt[1] = &layouts[1 % LENGTH(layouts)];
	strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
	return m;
}

void
destroynotify(XEvent *e)
{
	Client *c, *prev, *root;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	switch (wintoclient2(ev->window, &c, &root)) {
	case ClientRegular: /* fallthrough */
	case ClientSwallowee:
		unmanage(c, 1);
		break;
	case ClientSwallower:
		for (prev = root; prev->swallowedby != c; prev = prev->swallowedby);
		prev->swallowedby = NULL;

		if (c->swallowedby) {
			c->swallowedby->mon = root->mon;
			c->swallowedby->tags = root->tags;
			c->swallowedby->next = root->next;
			root->next = c->swallowedby;
			attachstack(c->swallowedby);
			focus(NULL);
			arrange(c->swallowedby->mon);
			XMapWindow(dpy, c->swallowedby->win);
			setclientstate(c->swallowedby, NormalState);
		}

		free(c);
		updateclientlist();
		break;
	}
}

/*
 * Remove client c from its monitor's client list
 */
void
detach(Client *c)
{
	Client **tc; /* stores pointer to 'next' field of clients */

	/* Find client whose 'next' points 'c' and link to 'c->next' instead. */
	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

/*
 * Remove client from its monitor's focus list
 */
void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	/* If 'c' is the selected client change focus to another [yes?] */
	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

/*
 * If dir > 0 (dir ≤ 0) returns the next (previous) monitor's handle
 */
Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) { /* if dir > 0 pick selmon-next */
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons) /* selmon == 0th element; pick last element */
		for (m = mons; m->next; m = m->next);
	else /* pick the element before selmon */
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

void
drawbar(Monitor *m)
{
	// TODO: Simplify drawbar()
	//       The current implementation is a leftover from the original
	//       which uses three color schemes.

	int x, w, sw, pad = 0;
	int boxs = drw->fonts->h / 9;
	int boxw = drw->fonts->h / 6 + 2;
	unsigned int i, occ = 0, urg = 0;
	Client *c;

	/* draw status first so it can be overdrawn by tags later */
	drw_setscheme(drw, scheme[SchemeNorm]);
	sw = TEXTW(stext) - lrpad/2 + statusrpad;
	if (m == selmon) { /* status is only drawn on selected monitor */
		drw_text(drw, m->ww - sw, 0, sw, bh, lrpad/2, stext, 0);
	} else {
		drw_setscheme(drw, scheme[SchemeNorm]);
		drw_rect(drw, m->ww - sw, 0, sw, bh, 1, 1);
	}

	for (c = m->clients; c; c = c->next) {
		occ |= c->tags;
		if (c->isurgent)
			urg |= c->tags;
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(tags[i]);
		drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
		drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
		if (occ & 1 << i)
			drw_rect(drw, x + boxs, boxs, boxw, boxw,
				m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
				urg & 1 << i);
		x += w;
	}
	w = blw = TEXTW(m->ltsymbol);
	drw_setscheme(drw, scheme[SchemeNorm]);
	x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

	if (m->sel && m->sel->swallowedby) {
		w = TEXTW(swalsymb);
		x = drw_text(drw, x, 0, w, bh, lrpad / 2, swalsymb, 0);
	}

	if ((w = m->ww - sw - x) > bh) { // larger than bar height? the fuck?
		if (m->sel) {
			drw_setscheme(drw, scheme[SchemeNorm]);
			pad = MAX(lrpad / 2, ((m->ww - (int)TEXTW(m->sel->name))/2 - x));
			drw_text(drw, x, 0, w, bh, pad, m->sel->name, 0);
		}
		else {
			drw_setscheme(drw, scheme[SchemeNorm]);
			drw_rect(drw, x, 0, w, bh, 1, 1);
		}
	}
	drw_map(drw, m->barwin, 0, 0, m->ww, bh);
}

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
}

void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	/* only draw the last contiguous expose indicated by count=0. */
	if (ev->count == 0 && (m = wintomon(ev->window)))
		drawbar(m);
}

int
fakesignal(void)
{
	/* Unsafe; Input is not checked for correct syntax */

	/* Syntax: <SEP><COMMAND>[<SEP><ARG>]... */
	static const char sep[] = "###";
	static const char prefix[] = "#!";

	size_t numsegments, numargs;
	char rootname[256];
	char *segments[16] = {0};

	/* Get root name, split by separator and find the prefix */
	if (!gettextprop(root, XA_WM_NAME, rootname, sizeof(rootname))
		|| strncmp(rootname, prefix, sizeof(prefix) - 1)) {
		return 0;
	}
	numsegments = split(rootname + sizeof(prefix) - 1, sep, segments, sizeof(segments));
	numargs = numsegments - 1; /* number of parameters to the "command" */

	if (!strcmp(segments[0], "swallowqueue")) {
		Window w;
		Client *c;

		/* Params: windowid, [class], [instance], [title] */
		if (numargs == 0) { /* need at least a window id */
			return 1;
		}
		w = strtoul(segments[1], NULL, 0);

		/* Only regular clients, i.e. clients not involved in a swallow are
		 * allowed to swallow. Nested swallowing will be implemented in the
		 * future. */
		switch (wintoclient2(w, &c, NULL)) {
		case ClientRegular: /* fallthrough */
		case ClientSwallowee:
			swalqueue(c, segments[2], segments[3], segments[4]);
			break;
		}
	}
	else if (!strcmp(segments[0], "swallow")) {
		Client *swer, *swee;
		Window winswer, winswee;

		/* Params: swallower, swallowee */
		if (numargs < 2) {
			return 1;
		}

		winswer = strtoul(segments[1], NULL, 0);
		winswee = strtoul(segments[2], NULL, 0);
		if (wintoclient2(winswer, &swer, NULL) != ClientSwallower
			&& wintoclient2(winswee, &swee, NULL) != ClientSwallower) {
			swal(swer, swee);
		}
		return 1;
	}

	return 1;
}

void
focus(Client *c)
{
	/* If no client or an invisible one is to be focused, attempt to find the
	 * next visible client on the currently selected monitor. */
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);

	/* Unfocus any possibly focused clients. */
	if (selmon->sel && selmon->sel != c)
		unfocus(selmon->sel, 0);

	if (c) {
		if (c->mon != selmon)
			selmon = c->mon;
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		/* XPM 4.3.2: Unlike changes to the window background, changes to
		 * window's border attributes are reflected immediately. */
		XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
		setfocus(c);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next) /* no next mon */
		return;
	if ((m = dirtomon(arg->i)) == selmon) /* next mon == current mon */
		return;
	unfocus(selmon->sel, 0);
	selmon = m;
	focus(NULL);
}

void
focusstack(const Arg *arg)
{
	Client *c = NULL, *i;

	if (!selmon->sel) {
		return;
	}

	if (arg->i > 0) {
		for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
		if (!c)
			for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
	}
	 else {
		for (i = selmon->clients; i != selmon->sel; i = i->next) {
			if (ISVISIBLE(i))
				c = i;
		}
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
					c = i;
	}
	if (c) {
		focus(c);
		restack(selmon);
	}
}

Atom
getatomprop(Client *c, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
		&da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		XFree(p);
	}
	return atom;
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

/*
 * Write 'w'indow property 'atom' to 'text' (limited to 'size' chars).
 */
int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->win, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for (i = 0; i < LENGTH(keys); i++)
			if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
						True, GrabModeAsync, GrabModeAsync);
	}
}

void
incnmaster(const Arg *arg)
{
	selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;
	/* If the client does not participate in the WM_DELETE_WINDOW protocol
	 * perform a default execution, otherwise send it the corresponding event.
	 * Window managers should not use XDestroyWindow() on a window that
	 * has WM_DELETE_WINDOW in its WM_PROTOCOLS property (XPM 12.3.3.2.3), why
	 * presumably applies to XKillClient().
	 * */
	if (!sendevent(selmon->sel, wmatom[WMDelete])) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

/*
 * Create a swallow instance and attach it to the top of the list of swallows.
 * 'class', 'inst' and 'title' shall point null-terminated strings or be NULL,
 * implying a wildcard. If 'c' corresponds to an existing swallow, the
 * swallow's filters are updated and no new swallow instance is created.
 * Complement to swalunqueue().
 */
void
swalqueue(Client *c, const char *class, const char *inst, const char *title)
{
	/*
	 * Unsafe; Caller must ensure that 'c'
	 *  - is not swallowing
	 *  - is not swallowed
	 * Nested swallowing will be implemented in the future.
	 */

	Swallow *s;

	if (!c)
		return;

	/* Update swallow filters for existing, queued swallower */
	for (s = swallows; s; s = s->next) {
		if (s->client == c) {
			if (class)
				strncpy(s->class, class, sizeof(s->class) - 1);
			else
				s->class[0] = '\0';
			if (inst)
				strncpy(s->inst, inst, sizeof(s->inst) - 1);
			else
				s->inst[0] = '\0';
			if (title)
				strncpy(s->title, title, sizeof(s->title) - 1);
			else
				s->title[0] = '\0';

			return;
		}
	}

	s = ecalloc(1, sizeof(Swallow));
	s->client = c;
	if (class)
		strncpy(s->class, class, sizeof(s->class) - 1);
	if (inst)
		strncpy(s->inst, inst, sizeof(s->inst) - 1);
	if (title)
		strncpy(s->title, title, sizeof(s->title) - 1);

	/* Attach new swallow at top of the list */
	s->next = swallows;
	swallows = s;
}

void
manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;
	c->cfact = 1.0;

	updatetitle(c); /* sets c->name */
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		/* If 'w' is a transient window (e.g. pop-up, dialogue box, ..) on behalf of an existing
		 * top-level window 'trans' map it to the same tags and monitors. See ICCCM 4.1.2.6. */
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = selmon;
		applyrules(c);
	}

	/*
	 * Prune extents of window. Relevant for floating windows as a tiling layout will
	 * override the geometry fields via arrange().
	 */
	if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
		c->x = c->mon->mx + c->mon->mw - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
		c->y = c->mon->my + c->mon->mh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->mx);
	/* only fix client y-offset, if the client center might cover the bar */
	c->y = MAX(c->y, ((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
		&& (c->x + (c->w / 2) < c->mon->wx + c->mon->ww)) ? bh : c->mon->my);
	c->bw = borderpx;

	/*
	 * Border configuration
	 */
	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);

	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c); /* fullscreen || floating */
	updatesizehints(c); /* initialize size hint fields */
	updatewmhints(c);
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow(dpy, c->win);
	attachbottom(c);
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */

	/* ICCCM 4.1.3.1: The window manager will palce a WM_STATE property on each
	 * top-level client window that is not in the withdrawn state. */
	setclientstate(c, NormalState);

	if (c->mon == selmon)
		unfocus(selmon->sel, 0);
	c->mon->sel = c;
	arrange(c->mon);
	XMapWindow(dpy, c->win);
	focus(NULL);
}

void
manageswallow(Client *swer, Window w)
{
	/* swer can be regular client or swallowee */
	Client *swee, **pc;
	XWindowChanges wc;

	/* Swallowing of and by fullscreen clients produces peculiar
	 * artefacts such as fullscreen terminals and pseudo-fullscreen
	 * application windows. We avoid that altogether by exiting
	 * fullscreen mode. */
	setfullscreen(swer, 0);

	swee = ecalloc(1, sizeof(Client));
	swee->win = w;
	swee->swallowedby = swer;

	/* Copy relevant fields from swallowing client. */
	swee->mon = swer->mon;
	swee->x = swee->oldx = swer->x;
	swee->y = swee->oldy = swer->y;
	swee->w = swee->oldw = swer->w;
	swee->h = swee->oldh = swer->h;
	swee->isfloating = swer->isfloating;
	swee->bw = swer->bw;
	swee->oldbw = swer->oldbw;
	swee->cfact = swer->cfact;
	swee->tags = swer->tags;

	updatetitle(swee);

	/* Swap in new client into the lists. */
	for (pc = &swer->mon->clients; *pc && *pc != swer; pc = &(*pc)->next);
	*pc = swee;
	swee->next = swer->next;
	detachstack(swer);
	attachstack(swee);

	/* Border config */
	wc.border_width = swee->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(swee);
	updatesizehints(swee);
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(swee, 0);
	if (swee->isfloating)
		XRaiseWindow(dpy, swee->win);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &(swee->win), 1);
	setclientstate(swee, NormalState);
	if (swee->mon == selmon)
		unfocus(selmon->sel, 0);
	swee->mon->sel = swee;

	/* Contrasting manage()'s implementation we need to explicitly resize the
	 * window which in manage()'s implementation is done via its call to
	 * arrange(). Deep down arrange() reconfigures the window only if the
	 * client's stored state (c->x, etc) does not match the desired geometry.
	 * As we've copied the configuration of an existing window, arrange() won't
	 * do anything here. Thus we call XMoveResize() explicitly and omit
	 * arrange(). */
	XMoveResizeWindow(dpy, swee->win, swee->x, swee->y, swee->w, swee->h);

	XMapWindow(dpy, swee->win);
	XUnmapWindow(dpy, swer->win);
	focus(NULL);
}

void
mappingnotify(XEvent *e)
{
	/* XPM 8.3.1: MappingNotify indicates that the keyboard mapping or pointer
	 * mapping has changed. This event is reported to all clients by the server
	 * when any client changes those mappings (e.g. via setxkbmap). */

	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	Client *c, *prev, *root;
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;
	Swallow *s;

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;

	/* By design of dwm, windows whose override_redirect flag is set shall not be
	 * managed by it. */
	if (wa.override_redirect)
		return;

	switch (wintoclient2(ev->window, &c, &root)) {
	case ClientRegular: /* fallthrough */
	case ClientSwallowee:
		/* should never happen; regulars and swallowees are always mapped. */
		return;
	case ClientSwallower:
		for (prev = root; prev->swallowedby != c; prev = prev->swallowedby);
		prev->swallowedby = NULL;

		c->mon = root->mon;
		c->tags = root->tags;
		c->isfloating = 0;

		c->next = root->next;
		root->next = c;
		attachstack(c);
		focus(NULL);
		arrange(c->mon);
		XMapWindow(dpy, c->win);
		setclientstate(c, NormalState);
		focus(NULL);
		return;
	}

	/* No client manages the new window. See if any swallow matches. */
	if (!(s = wintoswallow(ev->window))) {
		manage(ev->window, &wa);
	}
	else {
		manageswallow(s->client, ev->window);
		swalunqueue(s);
	}

}

/*
 * Apply monocle layout
 */
void
monocle(Monitor *m)
{
	Client *c;

	for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
		resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void
motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;

	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, 1);
		selmon = m;
		focus(NULL);
	}
	mon = m;
}

void
movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selmon->wx - nx) < snap)
				nx = selmon->wx;
			else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
				nx = selmon->wx + selmon->ww - WIDTH(c);
			if (abs(selmon->wy - ny) < snap)
				ny = selmon->wy;
			else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
				ny = selmon->wy + selmon->wh - HEIGHT(c);
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
			&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
				togglefloating(NULL);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, c->w, c->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

/*
 * Move clients through the client list
 */
void
moveclient(const Arg *arg) {
	Client *c = NULL, *p = NULL, *pc = NULL, *i;

	if(arg->i > 0) {
		/* find the client after selmon->sel */
		for(c = selmon->sel->next; c && (!ISVISIBLE(c) || c->isfloating); c = c->next);
		if(!c)
			for(c = selmon->clients; c && (!ISVISIBLE(c) || c->isfloating); c = c->next);

	}
	else {
		/* find the client before selmon->sel */
		for(i = selmon->clients; i != selmon->sel; i = i->next)
			if(ISVISIBLE(i) && !i->isfloating)
				c = i;
		if(!c)
			for(; i; i = i->next)
				if(ISVISIBLE(i) && !i->isfloating)
					c = i;
	}
	/* find the client before selmon->sel and c */
	for(i = selmon->clients; i && (!p || !pc); i = i->next) {
		if(i->next == selmon->sel)
			p = i;
		if(i->next == c)
			pc = i;
	}

	/* swap c and selmon->sel selmon->clients in the selmon->clients list */
	if(c && c != selmon->sel) {
		Client *temp = selmon->sel->next==c?selmon->sel:selmon->sel->next;
		selmon->sel->next = c->next==selmon->sel?c:c->next;
		c->next = temp;

		if(p && p != c)
			p->next = c;
		if(pc && pc != selmon->sel)
			pc->next = selmon->sel;

		if(selmon->sel == selmon->clients)
			selmon->clients = c;
		else if(c == selmon->clients)
			selmon->clients = selmon->sel;

		arrange(selmon);
	}
}

Client *
nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
	return c;
}

void
pop(Client *c)
{
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Swallow *s;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
		/* update status (unless a fake signal was received) */
		if (!fakesignal())
			updatestatus();
	}
	else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			// If an non-floating existing, managed window chooses to act on behalf of another
			// existing, managed window make the window float and rearrange tiles. lolwat?
			if (!c->isfloating && XGetTransientForHint(dpy, c->win, &trans)
				&& (c->isfloating = (wintoclient(trans) != NULL)))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints(c);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars(); /* Indicate new urgent windows */
			break;
		}
		/* Note that the atoms below are not included in the switch statement
		 * because their values are not compile-time constants (no built-in
		 * atoms). */
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			/* Update client's preferred display name */
			updatetitle(c); // TODO: As title now contains the classname this no longer makes sense.
			if (c == c->mon->sel)
				drawbar(c->mon);

			if (swalretroactive && (s = wintoswallow(c->win))) {
				swal(s->client, c);
			}
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
	}
}

void
quit(const Arg *arg)
{
	if(arg->i) restart = 1;
	running = 0;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
swalmouse(const Arg *arg)
{
	Client *swer, *swee;
	XEvent ev;

	if (!(swee = selmon->sel))
		return;

	if (XGrabPointer(dpy, root, False, ButtonPressMask|ButtonReleaseMask, GrabModeAsync,
		GrabModeAsync, None, cursor[CurSwal]->cursor, CurrentTime) != GrabSuccess)
		return;

	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest: /* fallthrough */
		case Expose: /* fallthrough */
		case MapRequest:
			handler[ev.type](&ev);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);

	/* TODO: Check if this switch-case can be removed since
	 *		 swallowers are never visible and cannot be selected by mouse. */
	switch (wintoclient2(ev.xbutton.subwindow, &swer, NULL)) {
		case ClientRegular: /* fallthrough */
		case ClientSwallowee:
			if (swer != swee) {
				swal(swer, swee);
			}
			break;
	}

	/* Remove accumulated pending EnterWindow events */
	XCheckMaskEvent(dpy, EnterWindowMask, &ev);
}

/*
 * Remove swallow instance from list of swallows and free its resources.
 * Complement to swalqueue(). If NULL is passed every swallow is
 * deleted from the list.
 */
void
swalunqueue(Swallow *s)
{
	Swallow *t, **ps;

	if (s) {
		for (ps = &swallows; *ps && *ps != s; ps = &(*ps)->next);
		*ps = s->next;
		free(s);
	}
	else {
		for(s = swallows; s; s = t) {
			t = s->next;
			free(s);
		}
		swallows = NULL;
	}
}

/*
 * Removes swallow queued for a specific client.
 */
void
swalunqueuebyclient(Client *c)
{
	Swallow *s;

	for (s = swallows; s; s = s->next) {
		if (c == s->client) {
			swalunqueue(s);
			break; /* max. 1 queued swallow per client */
		}
	}
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact)) {
		resizeclient(c, x, y, w, h);
	}
}

/*
 * Resize a client window (with immediate effect)
 */
void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}

void
resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest: /* fallthrough */
		case Expose: /* fallthrough */
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, c->x, c->y, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev)); /* Remove accumulated pending EnterWindow events */
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
restack(Monitor *m)
{
	// When does is become necessary to restack?
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	if (!m->sel)
		return;

	/* If selected window is floating raise it to the top. */
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);

	/* Apply stacking order as represented by the stack list to all tiled
	 * windows, beginning at the bar. */
	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext) {
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
		}
	}
	XSync(dpy, False);

	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev)); //What's that for?
}

void
run(void)
{
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while (running && !XNextEvent(dpy, &ev))
		if (handler[ev.type])
			handler[ev.type](&ev); /* call handler */
}

void
runstartup(void)
{
  system("~/.config/dwm/autostart-blocking.sh");
  system("~/.config/dwm/autostart.sh &");
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
				|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

/*
 * Sends client c to monitor m
 */
void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m) /* client's monitor is target monitor */
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;	/* set client's new monitor handle */
	c->tags = m->tagset[m->seltags]; /* make client adopt new monitor's displayed tags */
	c->next = NULL; /* Null c->next, as we're attaching to end of list */
	attachbottom(c);
	attachstack(c);
	focus(NULL);
	arrange(NULL);
}

/*
 * Change client's WM_STATE property
 */
void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Client *c, Atom proto)
{
	int n;
	Atom *protocols;
	int exists = 0;
	XEvent ev;

	if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		while (!exists && n--)
			exists = protocols[n] == proto;
		XFree(protocols);
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = c->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = proto;
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, c->win, False, NoEventMask, &ev);
	}
	return exists;
}

void
setfocus(Client *c)
{
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char *) &(c->win), 1);
	}

	/* If the client participates in WM_TAKE_FOCUS send the appropriate
	 * message. See XPM 12.3.3.2.1 */
	sendevent(c, wmatom[WMTakeFocus]);
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen){
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0;
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}

void
setgaps(const Arg *arg)
{
	if ((arg->i == 0) || (selmon->gappx + arg->i < 0))
		selmon->gappx = 0;
	else
		selmon->gappx += arg->i;
	arrange(selmon);
}

void
setlayout(const Arg *arg)
{
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = (Layout *)arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

/*
 * Control relative size of client in client area
 */
void setcfact(const Arg *arg) {
	float f;
	Client *c;

	c = selmon->sel;

	if(!arg || !c || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f + c->cfact;
	if(arg->f == 0.0)
		f = 1.0;
	else if(f < 0.25 || f > 4.0)
		return;
	c->cfact = f;
	arrange(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.1 || f > 0.9)
		return;
	selmon->mfact = f;
	arrange(selmon);
}

void
setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;

	/* clean up any zombies immediately */
	sigchld(0);

	signal(SIGHUP, sighup);
	signal(SIGTERM, sigterm);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	xinitvisual();
	drw = drw_create(dpy, screen, root, sw, sh, visual, depth, cmap);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;
	bh = barpady <= 1 ? drw->fonts->h * (1 + 2 * barpady) : drw->fonts->h + barpady;
	updategeom();

	/* Initialize atoms from property names */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	/* win title; UTF-8 substitute of XA_WM_NAME */
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);

	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	cursor[CurSwal] = drw_cur_create(drw, XC_bottom_side);
	/* init appearance */
	scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], alphas[i], 3);
	/* init bars */
	updatebars();
	updatestatus();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;

	/* XPM 16.2: The structure is the location, size, stacking order, border
	 * width, and mapping status of a window. The substructure is all these
	 * statistics about the children of a particular window. This is the
	 * complete set of information about screen layout that the window manager
	 * might need in order to implement its policy. Redirection means that an
	 * event is sent to the client selecting redirection (usually the window
	 * manager), and the original structure−changing request is not executed.
	 * */
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
}


void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void
showhide(Client *c)
{
	if (!c)
		return;

	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void
sigchld(int unused)
{
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("can't install SIGCHLD handler:");
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
sighup(int unused)
{
	Arg a = {.i = 1};
	quit(&a);
}

void
sigterm(int unused)
{
	Arg a = {.i = 0};
	quit(&a);
}

void
spawn(const Arg *arg)
{
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

/*
 * Perform swallow for two clients.
 */
void
swal(Client *swer, Client *swee)
{
	/* 'swer' and 'swee' must be regular or swallowee, but not swallower.  */

	Client *c, **pc;
	Swallow *s;

	/* Remove any queued swallows involving the participants. */
	if (swallows) {
		for (s = swallows; s; s = s->next) {
			if (swee == s->client || swer == s->client) {
				swalunqueue(s);
			}
		}
	}

	/* Disable fullscreen prior to swallow. Swallows involving fullscreen
	 * windows may produce quirky artefacts such as fullscreen terminals
	 * or pseudo-fullscreen windows. */
	setfullscreen(swer, 0);
	setfullscreen(swee, 0);

	detach(swee);
	detachstack(swee);
	detachstack(swer);

	/* Copy relevant fields into swee. */
	swee->tags = swer->tags;
	swee->mon = swer->mon;
	swee->x = swer->x;
	swee->y = swer->y;
	swee->w = swer->w;
	swee->h = swer->h;
	swee->oldx = swer->oldx;
	swee->oldy = swer->oldy;
	swee->oldw = swer->oldw;
	swee->oldh = swer->oldh;
	swee->isfloating = swer->isfloating;
	swee->bw = swer->bw;
	swee->oldbw = swer->oldbw;
	swee->cfact = swer->cfact;

	/* Append swer at the end of swee's swallow chain */
	for (c = swee; c->swallowedby; c = c->swallowedby);
	c->swallowedby = swer;

	/* Swap swallowee into stack */
	for (pc = &swer->mon->clients; *pc && *pc != swer; pc = &(*pc)->next);
	*pc = swee;
	swee->next = swer->next;
	attachstack(swee);

	XUnmapWindow(dpy, swer->win);
	arrange(NULL); /* redraw all monitors */
	XMoveResizeWindow(dpy, swee->win, swee->x, swee->y, swee->w, swee->h);
	focus(NULL); // TODO: keep focus unless focused client is swallower
}

/*
 * Stops active swallow for currently selected client
 */
void
swalstopsel(const Arg *arg)
{
	if (selmon->sel && selmon->sel->swallowedby)
		swalstop(selmon->sel);

	// TODO: Add arg to remove all active and queued swallows
}

void
statusclick(const Arg *arg)
{
	const unsigned mbutton = arg->ui >> (sizeof(unsigned) * CHAR_BIT - 3);
	const unsigned cindex = ((arg->ui << 3) >> 3);

	sprintf(statusclick_cindex, "%u", cindex);
	sprintf(statusclick_envs, "BUTTON=%u", mbutton);
	const Arg arg2 = { .v = statusclick_cmd };
	spawn(&arg2);
}

void
tag(const Arg *arg)
{
	if (selmon->sel && arg->ui & TAGMASK) {
		selmon->sel->tags = arg->ui & TAGMASK;
		focus(NULL);
		arrange(selmon);
	}
}

/*
 * Moves selected client to next/prev monitor
 */
void
tagmon(const Arg *arg)
{
	if (!selmon->sel || !mons->next) /* no active client || no next mon */
		return;
	Monitor *m = dirtomon(arg->i); /* get next mon */
	Client *c = selmon->sel;
	sendmon(c, m);
	focus(c);
	restack(c->mon); /* required for focus(c) to work */
}

void
tile(Monitor *m)
{
	unsigned int i, n, h, mw, my, ty;
	float mfacts = 0, sfacts = 0;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
		if (n < m->nmaster)
			mfacts += c->cfact;
		else
			sfacts += c->cfact;
	}
	if (n == 0)
		return;

	if (n > m->nmaster)
		mw = m->nmaster ? m->ww * m->mfact : 0;
	else
		mw = m->ww - m->gappx;

	for (i = 0, my = ty = m->gappx, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
		if (i < m->nmaster) {
			h = (m->wh - my) * (c->cfact / mfacts) - m->gappx;
			resize(c, m->wx + m->gappx, m->wy + my, mw - (2*c->bw) - m->gappx, h - (2*c->bw), 0);
			my += HEIGHT(c) + m->gappx;

			mfacts -= c->cfact;
		} else {
			h = (m->wh - ty) * (c->cfact / sfacts) - m->gappx;
			resize(c, m->wx + mw + m->gappx, m->wy + ty, m->ww - mw - (2*c->bw) - 2*m->gappx, h - (2*c->bw), 0);
			ty += HEIGHT(c) + m->gappx;

			sfacts -= c->cfact;
		}
	}
}

void
togglebar(const Arg *arg)
{
	selmon->showbar = !selmon->showbar;
	updatebarpos(selmon);
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, bh);
	arrange(selmon);
}

void
togglefloating(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
		return;
	selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
	if (selmon->sel->isfloating)
		resize(selmon->sel, selmon->sel->x, selmon->sel->y,
			selmon->sel->w, selmon->sel->h, 0);
	arrange(selmon);
}

void
toggletag(const Arg *arg)
{
	unsigned int newtags;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void
toggleview(const Arg *arg)
{
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;
		focus(NULL);
		arrange(selmon);
	}
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void
unmanage(Client *c, int destroyed)
{
	Client *swer;
	XWindowChanges wc;
	Monitor *m = c->mon;

	if ((swer = c->swallowedby)) {
		swer->mon = c->mon;
		swer->tags = c->tags;
		swer->cfact = c->cfact;
		swer->next = c->next;
		swer->isfloating = c->isfloating;
		c->next = swer;
		attachstack(swer);
		resizeclient(swer, c->x, c->y, c->w, c->h);
		XMapWindow(dpy, swer->win);
	}
	swalunqueuebyclient(c);

	/* Remove client from lists */
	detach(c);
	detachstack(c);

	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);

		// Why restore the border? And why not use XSetWindowBorderWidth() directly?
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}

	free(c);
	focus(NULL);
	updateclientlist();
	arrange(m);

}


void
unmapnotify(XEvent *e)
{

	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if ((c = wintoclient(ev->window))) {
		if (ev->send_event) {
			/* ICCCM 4.1.4:  When changing the state of the window to
			 * Withdrawn, the client must (in addition to unmapping the window)
			 * send a synthetic UnmapNotify event to the root using a SendEvent
			 * request. */
			setclientstate(c, WithdrawnState);
		}
		else
			unmanage(c, 0);
	}
}

/*
 * Stop an active swallow. Unswallows a swallowee, re-maps the swallower and
 * attaches it behind the swallowee.
 */
void
swalstop(Client *swee)
{
	Client *swer;

	if (!swee || !(swer = swee->swallowedby))
		return;

	swee->swallowedby = NULL;

	/* Set fields */
	swer->mon = swee->mon;
	swer->tags = swee->tags;

	/* Attach */
	swer->next = swee->next;
	swee->next = swer;
	attachstack(swer);

	/* Calculate geom and map */
	arrange(swer->mon);
	XMapWindow(dpy, swer->win);
}

void
updatebars(void)
{
	/* XPM 4.2: There are two structures associated with window attributes.
	 * XWindowAttributes is a read-only structure that contains all the
	 * attributes, while XSetWindowAttributes is a structure that only those
	 * attributes that a program is allowed to set. */

	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixel = 0,
		.border_pixel = 0,
		.colormap = cmap,
		.event_mask = ButtonPressMask|ExposureMask
	};
	XClassHint ch = {"dwm", "dwm"};
	for (m = mons; m; m = m->next) {
		if (m->barwin)
			continue;
		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, depth,
			InputOutput, visual,
			CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
		XMapRaised(dpy, m->barwin);
		XSetClassHint(dpy, m->barwin, &ch);
	}
}

void
updatebarpos(Monitor *m)
{
	/* Set window area size to full monitor size and subtract extent of bar if
	 * the bar is to be drawn. */
	m->wy = m->my;
	m->wh = m->mh;
	if (m->showbar) {
		m->wh -= bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh;
		m->wy = m->topbar ? m->wy + bh : m->wy;
	} else
		m->by = -bh;
}

void
updateclientlist()
{
	/* _NET_CLIENT_LIST contains all windows managed by the WM.
	 * Shouldn't _NET_CLIENT_LIST_STACKING be used aswell? According to EWMH
	 * spec: _NET_CLIENT_LIST has initial mapping order, starting with the
	 * oldest window. _NET_CLIENT_LIST_STACKING has bottom-to-top stacking
	 * order. */
	Client *c, *d;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			for (d = c; d; d = d->swallowedby) {
				XChangeProperty(dpy, root, netatom[NetClientList],
					XA_WINDOW, 32, PropModeAppend,
					(unsigned char *) &(c->win), 1);
			}
		}
	}
}

int
updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;
		if (n <= nn) { /* new monitors available */
			for (i = 0; i < (nn - n); i++) {
				for (m = mons; m && m->next; m = m->next);
				if (m)
					m->next = createmon();
				else
					mons = createmon();
			}
			for (i = 0, m = mons; i < nn && m; m = m->next, i++)
				if (i >= n
				|| unique[i].x_org != m->mx || unique[i].y_org != m->my
				|| unique[i].width != m->mw || unique[i].height != m->mh)
				{
					dirty = 1;
					m->num = i;
					m->mx = m->wx = unique[i].x_org;
					m->my = m->wy = unique[i].y_org;
					m->mw = m->ww = unique[i].width;
					m->mh = m->wh = unique[i].height;
					updatebarpos(m);
				}
		} else { /* less monitors available nn < n */
			for (i = nn; i < n; i++) {
				for (m = mons; m && m->next; m = m->next);
				while ((c = m->clients)) {
					dirty = 1;
					m->clients = c->next;
					detachstack(c);
					c->mon = mons;
					attachbottom(c);
					attachstack(c);
				}
				if (m == selmon)
					selmon = mons;
				cleanupmon(m);
			}
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

/*
 * Updates a client's size hint parameters
 */
void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;

	/* XPM 3.2.8: base_width/base_height takes priority over
	 * min_width/max_width. Only one of these pairs should be set. */
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;

	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;

	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;

	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;

	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;

	/* Client has fixed size if max = min > 0 in both dimensions */
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}

void
updatestatus(void)
{
	if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
		strcpy(stext, "dwm-"VERSION);
	drawbar(selmon);
}

void
updatetitle(Client *c)
{
	XClassHint ch = { NULL, NULL };

	if (!XGetClassHint(dpy, c->win, &ch) || !ch.res_class) {
		strcpy(c->name, broken); /* mark broken clients */
		return;
	}

	/* Display window class instead of lengthy name as window's title */
	strncpy(c->name, ch.res_class, sizeof(c->name) - 1);
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog])
		c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			/* If the active window fancies itself urgent, clear its urgency
			 * flag immediately, as there's no better measure to satisfy an
			 * urgent window than to give it focus. .. */
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else {
			/* .. otherwise simply set the urgency field. */
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		}

		/* XPM 12.3.1.4.2: confuses me :( */
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void
view(const Arg *arg)
{
	if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK)
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
	focus(NULL);
	arrange(selmon);
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

int
wintoclient2(Window w, Client **pc, Client **proot)
{
	Monitor *m;
	Client *c, *d;

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->win == w) {
				if (!c->swallowedby) {
					*pc = c;
					return ClientRegular;
				}
				else {
					*pc = c;
					return ClientSwallowee;
				}
			}
			else {
				for (d = c->swallowedby; d; d = d->swallowedby) {
					if (d->win == w) {
						if (proot) /* set root client of swallow chain */
							*proot = c;
						*pc = d;
						return ClientSwallower;
					}
				}
			}
		}
	}
	*pc = NULL;
	return 0; /* no match */
}

/*
 * Return swallow instance which targets window 'w' as determined by its class
 * name, instance name and window title. Returns NULL if none is found. Pendant
 * to wintoclient.
 */
Swallow *
wintoswallow(Window w)
{
	XClassHint ch = { NULL, NULL };
	Swallow *s = NULL;
	char title[sizeof(s->title)]; /* C Question: Determine a struct's member's static size */

	/* Retrieve the window's class name, instance name and title. */
	XGetClassHint(dpy, w, &ch);
	if (!gettextprop(w, netatom[NetWMName], title, sizeof(title)))
		gettextprop(w, XA_WM_NAME, title, sizeof(title));

	/* Search for matching swallow. Compare class, instance and title. Any
	 * unset property implies a wildcard */
	for (s = swallows; s; s = s->next) {
		if ((!ch.res_class || strstr(ch.res_class, s->class))
			&& (!ch.res_name || strstr(ch.res_name, s->inst))
			&& (title[0] == '\0' || strstr(title, s->title)))
			break;
	}

	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);

	return s;
}

Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

void
xinitvisual()
{
	XVisualInfo *infos;
	XRenderPictFormat *fmt;
	int nitems;
	int i;

	XVisualInfo tpl = {
		.screen = screen,
		.depth = 32,
		.class = TrueColor
	};
	long masks = VisualScreenMask | VisualDepthMask | VisualClassMask;

	infos = XGetVisualInfo(dpy, masks, &tpl, &nitems);
	visual = NULL;
	for(i = 0; i < nitems; i ++) {
		fmt = XRenderFindVisualFormat(dpy, infos[i].visual);
		if (fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
			visual = infos[i].visual;
			depth = infos[i].depth;
			cmap = XCreateColormap(dpy, root, visual, AllocNone);
			useargb = 1;
			break;
		}
	}

	XFree(infos);

	if (! visual) {
		visual = DefaultVisual(dpy, screen);
		depth = DefaultDepth(dpy, screen);
		cmap = DefaultColormap(dpy, screen);
	}
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;

	if (!selmon->lt[selmon->sellt]->arrange
	|| (selmon->sel && selmon->sel->isfloating))
		return;
	if (c == nexttiled(selmon->clients))
		if (!c || !(c = nexttiled(c->next)))
			return;
	pop(c);
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION);
	else if (argc != 1)
		die("usage: dwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display");
#ifdef XSYNCHRONIZE
	XSynchronize(dpy, 1);
#endif
	checkotherwm();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan(); /* load existing windows */
	runstartup();
	run();
	if(restart) execvp(argv[0], argv);
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}

// TODO(bug): Spurious tag change from urgent windows on different monitor
//            An urgent window on another monitor causes a change in displayed
//            tags on the currently active monitor.
//            How to reproduce:
//              1) Set a default monitor for an application (e.g. firefox)
//              2) On one monitor launch the application with a delay 'sleep 5;
//                 firefox google.com'
//              3) Switch to the other monitor and show any tag not associated
//                 with the application
//              => After sleep returns the dwm will switch tags on _both_
//                 monitors.

// TODO(bug): Resizing xephyr window seems to not propagate to dwm
//            Steps to reproduce:
//              1) Create a xephyr window and run dwm
//              2) increase xephyr's window size and create a few clients
//              3) clients cannot be focused by mouse click into the newly
//                 exposed area or the wrong client gets focused

// TODO(feat): Cycle layouts keybind and on click onto symbol

// TODO(fix): Add gaps to monocle

// TODO: Make -> CMake
// 		  - [ ] Remove makefile
// 		  - [ ] Install dwmswallow via cmake

// TODO(fix): man pages

// NOTE: dwm behaves differently inside Xephyr when using virtual monitors.
//		 check recttomon(). When drawing bars somehow every monitor thinks
//		 it's the selected.

// TODO: Check build with -Wall -pedantic

// TODO: valgrind memcheck

// TODO: Swallow features
//        - [x] delete swallows from list
//           - [x] by swallow 's'
//           - [x] all
//           - [x] by window (indirect: wintoswallow)
//        - [ ] unswallow: swalstop()
//           - [x] by client
//           - [x] by window (indirect)
//           - [x] selected client (hotkey)
//           - [ ] all clients
//        - [x] swallow (active) clients by window
//        - [x] enum for types 0 - 3 of wintoclient2
//        - [x] Swallow existing clients by cursor selection (Shift+mod -> move into swallower)
//        - [x] Designate acive swallowed window by icon 👅
//        - [x] Leave fullscreen prior to swallow
//        - [x] Refactor CLI to match swal** naming scheme
//        - [x] retroactive swallow (check swallows when wmname changes; req. for Zathura)
//        - [x] nested swallow
//        - TEST: What happens if a swallowee gets unmapped/destroyed?
//        - TEST: Swallow on multiple monitors
//        - TEST: Run in release mode (no XSYNCHRONIZE)
//        - TEST: Fullscreen swallows
//        - TEST: Floating swallows
//        - TEST: Swallow windows on different tags (and different monitors)
//        - TEST: Should window extend of remapped swallowers be pruned?
//
// BUGS: Swallow
// - [ ] Focus after stopped swallow shall be on swallowee; Seems random.
//    (differs between master and slave windows)
// - [ ] Stopped swallows for master clients created from queue (manageswallow)
//    produce two windows with a highlighted border.
//    Steps to reproduce:
//     - Create terminal on empty tag
//     - dwmswallow $WINDOWID; zathura
//     - Ctrl-u (stop swap)
//

// Questions:
//  - Killing a client always produces multiple unmap and destroy notifications. Why?
//  - Killing a client causes an unmap before a destroy. Why?