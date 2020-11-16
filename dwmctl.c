#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>

#include "util.h"

void
usage()
{
	printf(
		"Usage:\n"
		"\tswallow <WID> [-i INSTANCE] [-c CLASS]\n"
	);
}

int
main(int argc, char **argv)
{
	Display *dpy;
	int opt, screen;
	Window w, root;
	char class[256] = "\0", inst[256] = "\0", buf[768];

	if (argc >= 3 && !strcmp(argv[1], "swallow")) {

		static const char prefix[] = "#!";
		static const char argsep[] = "'+_+'";

		w = strtoul(argv[2], NULL, 0);
		while ((opt = getopt(argc - 2, argv + 2, "c:i:")) != -1) {
			switch (opt) {
			case 'c':
				strncpy(class, optarg, sizeof(class) - 1);
				class[sizeof(class) - 1] = '\0';
				break;
			case 'i':
				strncpy(inst, optarg, sizeof(inst) - 1);
				class[sizeof(inst) - 1] = '\0';
				break;
			}
		}

		if (!(dpy = XOpenDisplay(NULL))) {
			die("Cannot open display.");
		}
		screen = DefaultScreen(dpy);
		root = RootWindow(dpy, screen);
		snprintf(buf, sizeof(buf), "%s%s%s%lu%s%s%s%s",
			prefix, "swallow", argsep, w, argsep, class, argsep, inst);
		XStoreName(dpy, root, buf);
		XCloseDisplay(dpy);
	}
	else {
		die("Invalid arguments");
	}

	return 0;
}