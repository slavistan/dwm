/* See LICENSE file for copyright and license details. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	if (!(p = calloc(nmemb, size)))
		die("calloc:");
	return p;
}

void
die(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

void
infof(const char *fmt, ...) {
	FILE* fp;
	va_list ap;

	if ((fp = fopen(logfilepath, "a")) == NULL)
		die("fopen() failed:");
	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
  fclose(fp);
}

/* Stringify a client */
size_t
strnfy_client(char* str, size_t size, Client *c) {

  return snprintf(str, size,
      "Client 0x%p = {\n"
      "  name = '%s'\n"
      "  (mina, maxa) = (%f, %f)\n"
      "  cfact = %f\n"
      "  (x, y, w, h) = (%d, %d, %d, %d)\n"
      "  (oldx, oldy, oldw, oldh) = (%d, %d, %d, %d)\n"
      "  (basew, baseh, incw, inch, maxw, maxh, minw, minh) = (%d, %d, %d, %d, %d, %d, %d, %d)\n"
      "  (bw, oldbw) = (%d, %d)\n"
      "  tags = %u\n"
      "  (isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen) = (%d, %d, %d, %d, %d, %d)\n"
      "  next = 0x%p\n"
      "  snext = 0x%p\n"
      "  mon = 0x%p\n"
      "  win = %lu\n"
      "  swallowing = %p\n"
      "}\n"
      , (void*)c,
        c->name,
        c->mina, c->maxa,
        c->cfact,
        c->x, c->y, c->w, c->h,
        c->oldx, c->oldy, c->oldw, c->oldh,
        c->basew, c->baseh, c->incw, c->inch, c->maxw, c->maxh, c->minw, c->minh,
        c->bw, c->oldbw,
        c->tags,
        c->isfixed, c->isfloating, c->isurgent, c->neverfocus, c->oldstate, c->isfullscreen,
        (void*)c->next,
        (void*)c->snext,
        (void*)c->mon,
        c->win,
        c->swallowing);
}

void
logclient(Client *c, int verbosity) {

  if (!c)
    return;

  switch (verbosity) {
    case 0: { /* name */
      XTextProperty text;
      if (XGetClassHint(dpy, c->win, &text)) {
        infof("%p [#%lu, %s]", c, c->win, text.value);
        XFree(text.value);
      }
      else
        infof("%p [#%lu, NOWIN]", c, c->win);
      break;
    }

    default: { /* full info */
      char buf[1024];
      strnfy_client(buf, sizeof(buf), c);

      FILE* fp = fopen(logfilepath, "a");
      fprintf(fp, "%s", buf);
      fclose(fp);
      break;
    }
  }
}

void logclientlist(Client *first) {

  infof("Client list:\n");
	for (; first; first = first->next) {
    infof("\t");
    logclient(first, 0);
    infof("\n");
  }
}
