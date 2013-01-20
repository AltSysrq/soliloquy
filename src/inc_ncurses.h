/*
  Copyright ⓒ 2013 Jason Lingle

  This file is part of Soliloquy.

  Soliloquy is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Soliloquy is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Soliloquy.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef INC_NCURSES_H_
#define INC_NCURSES_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _XOPEN_SORUCE_EXTENDED
#define _XOPEN_SORUCE_EXTENDED
#endif

#if defined(HAVE_NCURSESW_CURSESW_H)
#include <ncursesw/cursesw.h>
#elif defined(HAVE_NCURSES_CURSESW_H)
#include <ncurses/cursesw.h>
#elif defined(HAVE_CURSESW_H)
#include <cursesw.h>
#elif defined(HAVE_NCURSESW_CURSES_H)
#include <ncursesw/curses.h>
#else
#include <curses.h>
#endif

#endif /* INC_NCURSES_H_ */
