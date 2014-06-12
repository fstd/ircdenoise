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
#include <unistd.h>

#include <libsrsirc/irc_ext.h>
#include <libsrsirc/irc_track.h>
#include <libsrsirc/util.h>

#include <libsrsbsns/io.h>
#include <libsrsbsns/addr.h>
#include <libsrsbsns/misc.h>

#include "dbg.h"

#include "msghandle.h"

#define DEF_CONTO_SOFT_MS 15000u
#define DEF_CONTO_HARD_MS 120000u
#define DEF_LOCAL_IF "0.0.0.0"
#define DEF_LOCAL_PORT 6776
#define DEF_VERB 1
#define DEF_ARM_TIME 300000u

int g_verb;

static struct settings_s {
	uint64_t conto_soft_us;
	uint64_t conto_hard_us;
	uint64_t arm_time;
	bool respawn;
	char localif[256];
	uint16_t localport;
} g_sett;

irc g_irc;
int g_clt;

static bool g_dumpplx;

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
			r = io_select2r(&canreadclt, &canreadirc, g_clt,
			    irc_sockfd(g_irc), 3000000u, false);
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




static void
process_args(int *argc, char ***argv, struct settings_s *sett)
{
	char *a0 = (*argv)[0];

	for (int ch; (ch = getopt(*argc, *argv, "i:T:a:rqvh")) != -1;) {
		switch (ch) {
		case 'i':
			ut_parse_hostspec(sett->localif, sizeof sett->localif,
				&sett->localport, NULL, optarg);
			WVX("will bind to '%s:%"PRIu16"'",
			    sett->localif, sett->localport);
			break;
		case 'T':
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
			break;
		case 'a':
			sett->arm_time =
			    (uint64_t)strtoull(optarg, NULL, 10) * 1000u;
			break;
		case 'r':
			sett->respawn = true;
			break;
		case 'q':
			g_verb--;
			break;
		case 'v':
			g_verb++;
			break;
		case 'h':
			usage(stdout, a0, EXIT_SUCCESS);
			break;
		case '?':default:
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

	g_verb = DEF_VERB;
	sett->conto_soft_us = DEF_CONTO_SOFT_MS*1000u;
	sett->conto_hard_us = DEF_CONTO_HARD_MS*1000u;
	strncpy(sett->localif, DEF_LOCAL_IF, sizeof sett->localif);
		sett->localif[sizeof sett->localif - 1] = '\0';
	sett->localport = DEF_LOCAL_PORT;
	sett->arm_time = DEF_ARM_TIME;

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

	msghandle_init(g_irc);
	msghandle_set_arm_time(sett->arm_time);

	WVX("initialized");
}

static void
usage(FILE *str, const char *a0, int ec)
{
	#define XSTR(s) STR(s)
	#define STR(s) #s
	#define U(STR) fputs(STR "\n", str)
	U("======================");
	U("== ircdenoise "PACKAGE_VERSION" ==");
	U("======================");
	fprintf(str, "usage: %s [-vhqriT] <hostspec>", a0);
	U("");
	U("\t<hostspec> specifies the IRC server to connect against");
	U("\t\thostspec := srvaddr[:port]");
	U("\t\tsrvaddr  := ip4addr|ip6addr|dnsname");
	U("\t\tport     := int(0..65535)");
	U("\t\tip4addr  := 'aaa.bbb.ccc.ddd'");
	U("\t\tip6addr  := '[aaaa:bbbb::eeee:ffff]'");
	U("\t\tdnsname  := 'irc.example.org'");
	U("");
	U("\t-r: Respawn on session end, rather than terminating");
	U("\t-v: Increase verbosity on stderr (use -vv or -vvv for more)");
	U("\t-q: Decrease verbosity on stderr (use -qq or -qqq for less)");
	U("\t-h: Display brief usage statement and terminate");
	U("");
	U("\t-a <int>: Time (ms) to stay `armed' after seeing a user talk\n"
	    "\t\t[def: "XSTR(DEF_ARM_TIME)"]");
	U("\t-i <hostspec>: Local interface and port to bind to.\n"
	    "\t\t[def: "DEF_LOCAL_IF":"XSTR(DEF_LOCAL_PORT)"]");
	U("\t-T <int>[:<int>]: Connect/Logon hard[:soft]-timeout in ms.\n"
	    "\t\t[def: "XSTR(DEF_CONTO_HARD_MS)":"XSTR(DEF_CONTO_SOFT_MS)"]");
	U("");
	U("(C) 2014, Timo Buhrmester (contact: #fstd @ irc.freenode.org)");
	#undef U
	#undef STR
	#undef XSTR
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
