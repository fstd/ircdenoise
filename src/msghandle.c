/* msghandle.c - (C) 2014, Timo Buhrmester
 * ircdenoise - irc denoise
 * See README for contact-, COPYING for license information. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <libsrsirc/irc_ext.h>

#include "msghandle.h"


extern irc g_irc;
extern int g_clt;


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


static bool
handle_PRIVMSG(irc h, tokarr *msg, size_t ac, bool pre)
{
	return true;
}

static bool
handle_JOIN(irc h, tokarr *msg, size_t ac, bool pre)
{
	return true;
}

static bool
handle_PART(irc h, tokarr *msg, size_t ac, bool pre)
{
	return true;
}

static bool
handle_QUIT(irc h, tokarr *msg, size_t ac, bool pre)
{
	return true;
}

static bool
handle_NICK(irc h, tokarr *msg, size_t ac, bool pre)
{
	return true;
}
