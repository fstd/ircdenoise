/* dbg.h - (C) 2014, Timo Buhrmester
 * ircdenoise - irc denoise
 * See README for contact-, COPYING for license information. */

#ifndef IRCDENOISE_DBG_H
#define IRCDENOISE_DBG_H 1

#include <stdlib.h>

#include <err.h>

extern int g_verb;

#define W_(FNC, THR, FMT, A...) do {                                  \
    if (g_verb < THR) break; \
    FNC("%s:%d:%s() - " FMT, __FILE__, __LINE__, __func__, ##A);  \
    } while (0)

#define W(FMT, A...) W_(warn, 1, FMT, ##A)
#define WV(FMT, A...) W_(warn, 2, FMT, ##A)

#define WX(FMT, A...) W_(warnx, 1, FMT, ##A)
#define WVX(FMT, A...) W_(warnx, 2, FMT, ##A)

#define E(FMT, A...) do { W_(warn, 0, FMT, ##A); \
    exit(EXIT_FAILURE);} while (0)

#define EX(FMT, A...) do { W_(warnx, 0, FMT, ##A); \
    exit(EXIT_FAILURE);} while (0)

#endif /* IRCDENOISE_DBG_H */
