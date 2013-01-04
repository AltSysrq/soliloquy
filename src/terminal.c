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

#include "terminal.slc"

#ifndef _XOPEN_SORUCE_EXTENDED
#define _XOPEN_SORUCE_EXTENDED 1
#endif
#include <ncursesw/curses.h>

subclass($c_Consumer,$c_Terminal)
member_of_domain($$d_Terminal, $d_Terminal)

defun($h_Terminal) {
  $$p_Terminal_screen = newterm($s_Terminal_type,
                                $p_Terminal_output, $p_Terminal_input);
  $i_Consumer_fd = fileno($p_Terminal_input);

  if (!$$p_Terminal_screen) {
    $y_Terminal_ok = false;
    // Remove from consumers since we're in an invalid state
    $f_Consumer_destroy();
    return;
  }

  start_color();
#ifdef HAVE_USE_DEFAULT_COLORS
  use_default_colors();
#endif

  $i_Terminal_num_colours = COLORS;
  $i_Terminal_num_colour_pairs = COLOR_PAIRS;
  $i_Terminal_rows = LINES;
  $i_Terminal_cols = COLS;

  $y_Terminal_ok = true;
  $f_Terminal_enter_raw_mode();
}

defun($h_Terminal_enter_raw_mode) {
  set_term($$p_Terminal_screen);
  raw();
  noecho();
  nonl();
  nodelay(stdscr, true);
  scrollok(stdscr, false);
  keypad(stdscr, true);
  meta(stdscr, true);
}

defun($h_Terminal_destroy) {
  set_term($$p_Terminal_screen);
  endwin();
  delscreen($$p_Terminal_screen);

  $f_Consumer_destroy();
}

defun($h_Terminal_read) {
  set_term($$p_Terminal_screen);
  while (-1 != ($i_Terminal_input_type = get_wch(&$i_Terminal_input_value))) {
    $f_Terminal_getch();
  }
  //TODO: EOF
}
