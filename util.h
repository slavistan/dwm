/* See LICENSE file for copyright and license details. */
#pragma once

#include "dwm.h"

#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B)        ((A) <= (X) && (X) <= (B))

extern char logfilepath[];
extern Display *dpy;

void die(const char *fmt, ...);
void infof(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);

size_t strnfy_client(char* str, size_t size, const Client*);
void logclient(const Client*, int verbosity);
