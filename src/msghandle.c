/* msghandle.c - (C) 2014, Timo Buhrmester
 * ircdenoise - irc denoise
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stddef.h>
#include <stdlib.h>

#include <libsrsirc/irc_ext.h>
#include <libsrsirc/irc_track.h>
#include <libsrsirc/util.h>

#include <libsrsbsns/strmap.h>
#include <libsrsbsns/misc.h>
#include <libsrsbsns/io.h>

#include "dbg.h"

#include "msghandle.h"

extern irc g_irc;
extern int g_clt;


static int s_interdast;
static uint64_t s_arm_time = 10000000u;

struct uent {
	uint64_t quietsincejoin;
	uint64_t lastmsg;
};

typedef struct uent uent;

struct chantag {
	smap_t membmap;
};

static bool handle_PRIVMSG(irc h, tokarr *msg, size_t ac, bool pre);
static bool handle_JOIN(irc h, tokarr *msg, size_t ac, bool pre);
static bool handle_PART(irc h, tokarr *msg, size_t ac, bool pre);
static bool handle_QUIT(irc h, tokarr *msg, size_t ac, bool pre);
static bool handle_NICK(irc h, tokarr *msg, size_t ac, bool pre);

void
msghandle_init(irc h)
{
	irc_reg_msghnd(h, "PRIVMSG", handle_PRIVMSG, true);
	irc_reg_msghnd(h, "JOIN", handle_JOIN, false);
	irc_reg_msghnd(h, "PART", handle_PART, true);
	irc_reg_msghnd(h, "QUIT", handle_QUIT, true);
	irc_reg_msghnd(h, "NICK", handle_NICK, true);
}

int
msghandle_interdast(int i)
{
	int old = s_interdast;
	s_interdast = i;
	return old;
}

void
msghandle_set_arm_time(uint64_t us)
{
	s_arm_time = us;
}

static uent*
mkuent(void)
{
	uent *u = malloc(sizeof *u);
	u->quietsincejoin = false;
	u->lastmsg = 0;
	return u;

}


static bool
handle_PRIVMSG(irc h, tokarr *msg, size_t ac, bool pre)
{
	char chname[256];
	ut_strtolower(chname, sizeof chname, (*msg)[2], irc_casemap(h));

	WVX("handling a PRIVMSG in '%s'", chname);

	if (chname[0] != '#') //XXX better ask libsrsirc
		return true;

	char nick[64];
	char lnick[64];
	ut_pfx2nick(nick, sizeof nick, (*msg)[0]);
	ut_strtolower(lnick, sizeof lnick, nick, irc_casemap(h));

	chanrep c;
	if (!irc_chan(h, &c, chname)) {
		WX("chan '%s' now known?!", chname);
		return true;
	}
	
	struct chantag *tag = c.tag;

	uent *u = smap_get(tag->membmap, lnick);
	if (!u) smap_put(tag->membmap, lnick, (u = mkuent()));

	if (u->quietsincejoin)
		s_interdast = 2;
	else if (!u->lastmsg)
		s_interdast = 1;

	u->lastmsg = (uint64_t)tstamp_us();
	WVX("updated lastmsg for '%s' in '%s'", lnick, chname);

	return true;
}

static bool
handle_JOIN(irc h, tokarr *msg, size_t ac, bool pre)
{
	char chname[256];
	ut_strtolower(chname, sizeof chname, (*msg)[2], irc_casemap(h));
	WVX("handling a JOIN in '%s'", chname);
	char nick[64];
	char lnick[64];
	ut_pfx2nick(nick, sizeof nick, (*msg)[0]);
	ut_strtolower(lnick, sizeof lnick, nick, irc_casemap(h));

	chanrep c;
	if (!irc_chan(h, &c, chname)) {
		WX("chan '%s' now known?!", chname);
		return true;
	}

	if (ut_istrcmp(nick, irc_mynick(h), irc_casemap(h)) == 0) {
		struct chantag *tag = malloc(sizeof *tag);
		tag->membmap = smap_init(256);
		irc_tag_chan(h, c.name, tag, false);
		return true;
	}

	userrep ur;
	if (!irc_user(h, &ur, lnick)) {
		WX("user '%s' not found", lnick);
		return true;
	}

	struct chantag *tag = c.tag;

	uent *u = smap_get(tag->membmap, lnick);
	if (!u) smap_put(tag->membmap, lnick, (u = mkuent()));

	if (u->lastmsg + s_arm_time > (uint64_t)tstamp_us())
		io_fprintf(g_clt, ":%s!%s@%s PRIVMSG %s :[denoise JOIN]\r\n",
		    ur.nick, ur.uname, ur.host, (*msg)[2]);

	u->quietsincejoin = true;

	return true;
}

static bool
handle_PART(irc h, tokarr *msg, size_t ac, bool pre)
{
	char chname[256];
	ut_strtolower(chname, sizeof chname, (*msg)[2], irc_casemap(h));
	WVX("handling a PART in '%s'", chname);
	char nick[64];
	char lnick[64];
	ut_pfx2nick(nick, sizeof nick, (*msg)[0]);
	ut_strtolower(lnick, sizeof lnick, nick, irc_casemap(h));

	chanrep c;
	if (!irc_chan(h, &c, chname)) {
		WX("chan '%s' now known?!", chname);
		return true;
	}
	
	if (ut_istrcmp(nick, irc_mynick(h), irc_casemap(h)) == 0) {
		struct chantag *tag = c.tag;
		void *e;
		if (smap_first(tag->membmap, NULL, &e))
			do {
				free(e);
			} while (smap_next(tag->membmap, NULL, &e));

		smap_dispose(tag->membmap);
		free(tag);
		return true;
	}

	userrep ur;
	if (!irc_user(h, &ur, lnick)) {
		WX("user '%s' not found", lnick);
		return true;
	}

	struct chantag *tag = c.tag;

	uent *u = smap_get(tag->membmap, lnick);
	if (!u)
		return true;

	if (u->lastmsg + s_arm_time > (uint64_t)tstamp_us())
		io_fprintf(g_clt, ":%s!%s@%s PRIVMSG %s :[denoise PART (%s)]"
		    "\r\n", ur.nick, ur.uname, ur.host, (*msg)[2],
		    (*msg)[3] ? (*msg)[3] : "");

	return true;
}

static bool
handle_QUIT(irc h, tokarr *msg, size_t ac, bool pre)
{
	char nick[64];
	char lnick[64];
	ut_pfx2nick(nick, sizeof nick, (*msg)[0]);
	ut_strtolower(lnick, sizeof lnick, nick, irc_casemap(h));
	WVX("handling a QUIT of '%s'", lnick);

	size_t nchans = irc_num_chans(h);
	chanrep *chans = malloc(nchans * sizeof *chans);

	irc_all_chans(h, chans, nchans);

	userrep ur;
	if (!irc_user(h, &ur, lnick)) {
		WX("user '%s' not found", lnick);
		return true;
	}

	for (size_t i = 0; i < nchans; i++) {
		struct chantag *tag = chans[i].tag;

		uent *u = smap_get(tag->membmap, lnick);
		if (!u)
			continue;

		if (u->lastmsg + s_arm_time > (uint64_t)tstamp_us())
			io_fprintf(g_clt, ":%s!%s@%s PRIVMSG %s :[denoise QUIT"
			    " (%s)]\r\n", ur.nick, ur.uname, ur.host,
			    chans[i].name, (*msg)[2] ? (*msg)[2] : "");
	}

	free(chans);

	return true;
}

static bool
handle_NICK(irc h, tokarr *msg, size_t ac, bool pre)
{
	char nick[64];
	char lnick[64];
	char lnnick[64];
	ut_pfx2nick(nick, sizeof nick, (*msg)[0]);
	ut_strtolower(lnick, sizeof lnick, nick, irc_casemap(h));
	ut_strtolower(lnnick, sizeof lnnick, (*msg)[2], irc_casemap(h));
	if (ut_istrcmp(nick, irc_mynick(h), irc_casemap(h)) == 0)
		return true;

	WVX("handling a NICK of '%s' (to '%s')", lnick, lnnick);

	userrep ur;
	if (!irc_user(h, &ur, lnick)) {
		WX("user '%s' not found", lnick);
		return true;
	}

	size_t nchans = irc_num_chans(h);
	chanrep *chans = malloc(nchans * sizeof *chans);

	irc_all_chans(h, chans, nchans);

	for (size_t i = 0; i < nchans; i++) {
		struct chantag *tag = chans[i].tag;

		uent *u = smap_get(tag->membmap, lnick);
		if (!u)
			continue;

		smap_del(tag->membmap, lnick);
		smap_put(tag->membmap, lnnick, u);

		if (u->lastmsg + s_arm_time > (uint64_t)tstamp_us())
			io_fprintf(g_clt, ":%s!%s@%s PRIVMSG %s :[denoise NICK"
			    " (%s)]\r\n", ur.nick, ur.uname, ur.host,
			    chans[i].name, (*msg)[2]);
	}

	free(chans);

	return true;
}
