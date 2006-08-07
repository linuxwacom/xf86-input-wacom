/*****************************************************************************
** wcmAction.h
**
** Copyright (C) 2006 - Andrew Zabolotny
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
**
** REVISION HISTORY
**   2006-07-17 0.0.1 - Initial release
*/

/* This pseudo-header file is included both from the X11 driver, and from
 * tools (notably xsetwacom). The reason is to have the function defined
 * in one place, to avoide desyncronization issues between two pieces of
 * almost identical code.
 */

#ifndef WCMACTION_X11_DRIVER
#  define xf86Msg   fprintf
#  define X_ERROR   stderr
#  define X_WARNING stderr
#endif

static int xf86WcmDecodeAction (const char *dev, const char *but, const char *ev)
{
	static struct
	{
		const char *keyword;
		unsigned mask;
		unsigned ent_mask;
	} action_flags [] =
	{
		{ "CORE",       AC_CORE,       AC_CORE    },
		{ "KEY",        AC_KEY,        AC_TYPE    },
		{ "BUTTON",     AC_BUTTON,     AC_TYPE    },
		{ "MODETOGGLE", AC_MODETOGGLE, AC_TYPE    },
		{ "DBLCLICK",   AC_DBLCLICK,   AC_TYPE    },
		{ "SHIFT",      AC_SHIFT,      AC_SHIFT   },
		{ "CONTROL",    AC_CONTROL,    AC_CONTROL },
		{ "CTRL",       AC_CONTROL,    AC_CONTROL },
		{ "META",       AC_META,       AC_META    },
		{ "ALT",        AC_ALT,        AC_ALT     },
		{ "SUPER",      AC_SUPER,      AC_SUPER   },
		{ "HYPER",      AC_HYPER,      AC_HYPER   },
	};

	int i, butev = 0, entev = 0;
	const char *orig_ev = ev;
	char tmp [128];

	for (;;)
	{
		while (*ev == ' ' || *ev == '\t')
			ev++;

		for (i = 0; i < sizeof (action_flags) / sizeof (action_flags [0]); i++)
		{
			int sl = strlen (action_flags [i].keyword);
			if ((ev [sl] == 0 || ev [sl] == ' ' || ev [sl] == '\t') &&
			    !strncasecmp (ev, action_flags [i].keyword, sl))
			{
				if (entev & action_flags [i].ent_mask)
				{
					size_t tmpsl = ev - orig_ev;
					if (tmpsl >= sizeof (tmp))
						tmpsl = sizeof (tmp) - 1;
					memcpy (tmp, orig_ev, tmpsl);
                                        tmp [tmpsl] = 0;
					xf86Msg (X_WARNING,
						 "%s: conflicting flags for %s: \"%s<<%s>>%s\"!\n",
						 dev, but, tmp, action_flags [i].keyword, ev + sl);
				}
				butev = (butev & ~action_flags [i].ent_mask) |
					action_flags [i].mask;
				entev |= action_flags [i].ent_mask;
				ev += sl;
				break;
			}
		}

		if (i >= sizeof (action_flags) / sizeof (action_flags [0]))
			break;
	}

	if (*ev)
	{
		char *end;
		int n = strtol (ev, &end, 0);
		if (n && end != ev)
		{
			/**
			 * No protection against (n & ~AC_CODE) != 0
			 * This is done intentionally so that you can use
			 * plain numerical values to assign a whole action
			 * at once; this is useful e.g. for saving/restoring
			 * button configs.
			 */
			butev |= n;
			ev = end;
		}
#ifndef WCMACTION_X11_DRIVER
		else
		{
			const char *end = ev + strcspn (ev, " \t");
			i = end - ev;
			if (i >= sizeof (tmp))
				i = sizeof (tmp) - 1;
			memcpy (tmp, ev, i);
			tmp [i] = 0;
			n = XStringToKeysym (tmp);
			if (n)
			{
				butev = (butev & ~AC_CODE) | (n & AC_CODE);
				ev = end;
			}
		}
#endif
	}

	while (*ev == ' ' || *ev == '\t')
		ev++;

	if (*ev)
	{
		size_t tmpsl = ev - orig_ev;
		if (tmpsl >= sizeof (tmp))
			tmpsl = sizeof (tmp) - 1;
		memcpy (tmp, orig_ev, tmpsl);
		tmp [tmpsl] = 0;
		xf86Msg (X_ERROR, "%s: invalid %s value: \"%s<<%s>>\"!\n",
			 dev, but, tmp, ev);
		return 0;
	}

	return butev;
}
