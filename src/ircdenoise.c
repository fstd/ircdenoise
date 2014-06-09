/* ircdenoise.c - (C) 2014, Timo Buhrmester
 * ircdenoise - irc denoise
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <getopt.h>
#include <err.h>

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
	int verb;
} g_sett;

static void process_args(int *argc, char ***argv, struct settings_s *sett);
static void init(int *argc, char ***argv, struct settings_s *sett);
static void usage(FILE *str, const char *a0, int ec);


static void
process_args(int *argc, char ***argv, struct settings_s *sett)
{
	char *a0 = (*argv)[0];

	for (int ch; (ch = getopt(*argc, *argv, "qvh")) != -1;) {
		switch (ch) {
		case 'q':
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
	sett->verb = DEF_VERB;
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
