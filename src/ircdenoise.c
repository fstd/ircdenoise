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
#include <signal.h>
#include <inttypes.h>
#include <sys/time.h>
#include <err.h>
#include <sys/select.h>

#include <libsrsirc/irc_ext.h>
#include <libsrsirc/irc_track.h>
#include <libsrsirc/util.h>

#include <libsrsbsns/io.h>
#include <libsrsbsns/addr.h>

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
static int g_clt;
static bool g_dumpplx;

static int select2(bool *rdbl1, bool *rdbl2, int fd1, int fd2, uint64_t to_us);
static uint64_t timestamp_us(void);
static void tconv(struct timeval *tv, uint64_t *ts, bool tv_to_ts);
static void process_args(int *argc, char ***argv, struct settings_s *sett);
static void init(int *argc, char ***argv, struct settings_s *sett);
static bool session(void);
static bool process_clt(void);
static bool process_irc(void);
static bool handle_cltmsg(const char *line);
static bool handle_ircmsg(tokarr *tok);
static void usage(FILE *str, const char *a0, int ec);


static bool
session(void)
{
	WVX("connecting to ircd in dumb mode");
	if (!irc_connect(g_irc)) {
		WX("failed to connect");
		io_fprintf(g_clt, ":de.noise ERROR :(denoise) Failed to "
		    "connect to '%s:%" PRIu16"': %s\r\n", irc_get_host(g_irc),
		    irc_get_port(g_irc), strerror(errno));
		return true;
	}

	WVX("connected, entering session loop");

	while (irc_online(g_irc)) {
		bool canreadclt = false;
		bool canreadirc = irc_can_read(g_irc);

		int r;
		if (!canreadirc) {
			r = select2(&canreadclt, &canreadirc, g_clt,
			    irc_sockfd(g_irc), 3000000u);
			if (r < 0 && errno != EINTR)
				E("select");
		}

		if (g_dumpplx) {
			irc_dump(g_irc);
			if (irc_tracking_enab(g_irc))
				trk_dump(g_irc, true);
			g_dumpplx = false;
		}

		if (canreadirc && !process_irc()) {
			io_fprintf(g_clt, ":de.noise ERROR :(denoise) "
			    "Connection dropped by IRCD\r\n");
			break;
		}

		if (canreadclt && !process_clt())
			break;
	}

	irc_reset(g_irc);
	return true;
}

static bool
process_clt(void)
{
	char buf[4096];
	int r = io_read_line(g_clt, buf, sizeof buf);
	if (r <= 0) {
		W("io_read_line");
		return false;
	}

	char *end = buf + strlen(buf) - 1;
	while (end >= buf && (*end == '\r' || *end == '\n'))
		*end-- = '\0';

	if (strlen(buf) == 0)
		return true;

	return handle_cltmsg(buf);
}

static bool
process_irc(void)
{
	tokarr tok;
	int r = irc_read(g_irc, &tok, 10000);

	if (r == -1) {
		WX("irc read failed");
		return false;
	} else if (r == 0)
		return true;

	return handle_ircmsg(&tok);
}

static bool
handle_cltmsg(const char *line)
{
	WVX("CLT -> IRCD: '%s'", line);
	irc_write(g_irc, line);
	return true;
}


static bool
handle_ircmsg(tokarr *tok)
{
	char line[1024];
	ut_snrcmsg(line, sizeof line, tok, irc_colon_trail(g_irc));
	WVX("IRCD -> CLT: '%s'", line);
	io_fprintf(g_clt, "%s\r\n", line);
	return true;
}



static uint64_t
timestamp_us(void)
{
	struct timeval t;
	uint64_t ts = 0;
	if (gettimeofday(&t, NULL) != 0)
		E("gettimeofday");
	else
		tconv(&t, &ts, true);

	return ts;
}

static void
tconv(struct timeval *tv, uint64_t *ts, bool tv_to_ts)
{
	if (tv_to_ts)
		*ts = (uint64_t)tv->tv_sec * 1000000u + tv->tv_usec;
	else {
		tv->tv_sec = *ts / 1000000u;
		tv->tv_usec = *ts % 1000000u;
	}
}

static int
select2(bool *rdbl1, bool *rdbl2, int fd1, int fd2, uint64_t to_us)
{
	fd_set read_set;
	struct timeval tout;
	int ret;

	uint64_t tsend = to_us ? timestamp_us() + to_us : 0;
	uint64_t trem = 0;

	if (fd1 < 0 && fd2 < 0) {
		W("both filedescriptors -1");
		return -1;
	}

	int maxfd = fd1 > fd2 ? fd1 : fd2;

	FD_ZERO(&read_set);

	if (fd1 >= 0)
		FD_SET(fd1, &read_set);
	if (fd2 >= 0)
		FD_SET(fd2, &read_set);

	for (;;) {
		if (tsend) {
			trem = tsend - timestamp_us();
			if (trem <= 0)
				trem = 1;
			tconv(&tout, &trem, false);
		}
		errno=0;
		ret = select(maxfd + 1, &read_set, NULL, NULL,
		    tsend ? &tout : NULL);

		if (ret == -1) {
			if (errno != EINTR)
				W("select failed");
			return -1;
		}

		break;
	}

	if (rdbl1) *rdbl1 = false;
	if (rdbl2) *rdbl2 = false;
	if (ret > 0) {
		if (fd1 >= 0 && FD_ISSET(fd1, &read_set))
			if (rdbl1) *rdbl1 = true;
		if (fd2 >= 0 && FD_ISSET(fd2, &read_set))
			if (rdbl2) *rdbl2 = true;
	}

	return ret;
}



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

	if (*argc != 1)
		EX("no or too many servers given");

	char host[256];
	uint16_t port;
	ut_parse_hostspec(host, sizeof host, &port, NULL, (*argv)[0]);

	irc_set_server(g_irc, host, port);
	WVX("set server to '%s:%"PRIu16"'",
	    irc_get_host(g_irc), irc_get_port(g_irc));

	irc_set_connect_timeout(g_irc,
	    g_sett.conto_soft_us, g_sett.conto_hard_us);

	WVX("initialized");
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

void
cleanup(void)
{
	if (g_irc)
		irc_dispose(g_irc);
}

void
infohnd(int s)
{
	g_dumpplx = true;
}

int
main(int argc, char **argv)
{
	init(&argc, &argv, &g_sett);
	atexit(cleanup);
	signal(DUMPSIG, infohnd);

	int lsck = addr_bind_socket_p(g_sett.localif, g_sett.localport,
	    NULL, NULL, g_sett.conto_soft_us, g_sett.conto_hard_us);

	if (lsck == -1)
		E("failed to bind");
	else if (listen(lsck, 16) != 0)
		E("failed to listen");

	do {
		if ((g_clt = accept(lsck, NULL, NULL)) == -1)
			E("failed to accept");

		WVX("accepted a socket, starting session");

		session();

		WVX("session over");

		close(g_clt);
	} while (g_sett.respawn);

	return EXIT_SUCCESS;
}
