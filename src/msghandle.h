/* msghandle.h - (C) 2014, Timo Buhrmester
 * ircdenoise - irc denoise
 * See README for contact-, COPYING for license information. */

#ifndef IRCDENOISE_MSGHANDLE_H
#define IRCDENOISE_MSGHANDLE_H 1

void msghandle_init(irc h);
bool msghandle_interdast(bool i);
void msghandle_set_arm_time(uint64_t us);

#endif /* IRCDENOISE_MSGHANDLE_H */
