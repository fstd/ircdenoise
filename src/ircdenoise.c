/* ircdenoise.c - (C) 2014, Timo Buhrmester
 * ircdenoise - irc denoise
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <getopt.h>
#include <inttypes.h>
#include <err.h>

#include <libsrsirc/irc_ext.h>
#include <libsrsirc/irc_track.h>
#include <libsrsirc/util.h>

#define DEF_CONTO_SOFT_MS 15000u
#define DEF_CONTO_HARD_MS 120000u
#define DEF_LOCAL_IF "0.0.0.0"
#define DEF_LOCAL_PORT 6776
#define DEF_VERB 1

#define W_(FNC, THR, FMT, A...) do {                                  \
    if (g_sett.verb < THR) break; \
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

static struct settings_s {
	uint64_t conto_soft_us;
	uint64_t conto_hard_us;
	bool respawn;
	int verb;
	char localif[256];
	uint16_t localport;
} g_sett;

static irc g_irc;
static void process_args(int *argc, char ***argv, struct settings_s *sett);
static void init(int *argc, char ***argv, struct settings_s *sett);
static void usage(FILE *str, const char *a0, int ec);


static void
process_args(int *argc, char ***argv, struct settings_s *sett)
{
	char *a0 = (*argv)[0];

	for (int ch; (ch = getopt(*argc, *argv, "i:T:rqvh")) != -1;) {
		switch (ch) {
		      case 'i':
			ut_parse_hostspec(sett->localif, sizeof sett->localif,
				&sett->localport, NULL, optarg);
			WVX("will bind to '%s:%"PRIu16"'",
			    sett->localif, sett->localport);
		break;case 'T':
			{
			char *arg = strdup(optarg);
			if (!arg)
				E("strdup failed");

			char *ptr = strchr(arg, ':');
			if (!ptr)
				sett->conto_hard_us =
				    (uint64_t)strtoull(arg, NULL, 10) * 1000u;
			else {
				*ptr = '\0';
				sett->conto_hard_us =
				    (uint64_t)strtoull(arg, NULL, 10) * 1000u;
				sett->conto_soft_us =
				    (uint64_t)strtoull(ptr+1, NULL, 10) * 1000u;
			}

			WVX("set connect timeout to %"PRIu64"ms (soft), "
			    "%"PRIu64"ms (hard)", sett->conto_soft_us / 1000u,
			    sett->conto_hard_us / 1000u);

			free(arg);
			}
		break;case 'r':
			sett->respawn = true;
		break;case 'q':
			sett->verb--;
		break;case 'v':
			sett->verb++;
		break;case 'h':
			usage(stdout, a0, EXIT_SUCCESS);
		break;case '?':default:
			usage(stderr, a0, EXIT_FAILURE);
		}
	}

	*argc -= optind;
	*argv += optind;
}


static void
init(int *argc, char ***argv, struct settings_s *sett)
{
	if (!(g_irc = irc_init()))
		EX("failed to initialize irc object");

	irc_set_dumb(g_irc, true);
	irc_set_track(g_irc, true);

	sett->verb = DEF_VERB;
	sett->conto_soft_us = DEF_CONTO_SOFT_MS*1000u;
	sett->conto_hard_us = DEF_CONTO_HARD_MS*1000u;
	strncpy(sett->localif, DEF_LOCAL_IF, sizeof sett->localif);
		sett->localif[sizeof sett->localif - 1] = '\0';
	sett->localport = DEF_LOCAL_PORT;

	process_args(argc, argv, sett);
}


static void
usage(FILE *str, const char *a0, int ec)
{
	#define I(STR) fputs(STR "\n", str)
	I("==============================");
	I("== ircdenoise - ha ha ha disregard that, i suck cocks ==");
	I("==============================");
	fprintf(str, "usage: %s [-h]\n", a0);
	I("");
	I("\t-h: Display brief usage statement and terminate");
	I("");
	I("(C) 2014, Timo Buhrmester (contact: #fstd on irc.freenode.org)");
	#undef I
	exit(ec);
}


int
main(int argc, char **argv)
{
	init(&argc, &argv, &g_sett);

	return EXIT_SUCCESS;
}
