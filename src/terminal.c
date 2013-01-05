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

/*
  TITLE: Terminal Input/Output Control
  DESCRIPTION: Manages the connect terminals, including the ncurses interface.
*/

#include "terminal.slc"

#ifndef _XOPEN_SORUCE_EXTENDED
#define _XOPEN_SORUCE_EXTENDED 1
#endif
#include <ncursesw/curses.h>

/*
  SYMBOL: $c_Terminal
    Encapsulates the state of a single connected terminal. $p_Terminal_input,
    $p_Terminal_output, and $s_Terminal_type *must* be specified when the
    constructor is called. Call $f_Terminal_destroy when a terminal is to be
    disconnected. Subclass of $c_Consumer.

  SYMBOL: $lo_terminals
    A list of all currently-initialised Terminal objects, in no particular
    order.

  SYMBOL: $s_Terminal_type
    The type of the connected terminal; that is, the contents of its TERM
    environment variable.

  SYMBOL: $p_Terminal_input
    A FILE* used for terminal input. It is owned strictly by this Terminal, and
    will be fclose()d when the Terminal is destroyed.

  SYMBOL: $p_Terminal_output
    A FILE* used for terminal output. It is owned strictly by this Terminal,
    and will be fclose()d when the Terminal is destroyed.

  SYMBOL: $y_Terminal_ok
    Set to true in the Terminal constructor if initialisation is
    successful. If it is false, the terminal has not been initialised and
    should be simply discarded (though the FILE* handles in $p_Terminal_input
    and $p_Terminal_output have not been closed).

  SYMBOL: $i_Terminal_num_colours $i_Terminal_num_colour_pairs
    These reflect the values of the curses variables COLORS and COLOR_PAIRS at
    the time the terminal was created. See color(3ncurses).

  SYMBOL: $i_Terminal_rows $i_Terminal_cols
    The current number of rows (lines) or columns present on the Terminal.
*/

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

  $lo_terminals = cons_o($o_Terminal, $lo_terminals);
}

/*
  SYMBOL: $h_Terminal_enter_raw_mode
    Called to place the terminal into "raw" mode. This typically happens in the
    Terminal's constructor, but may also be as a result of having suspended
    Soliloquy in one terminal.
 */
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

/*
  SYMBOL: $h_Terminal_destroy
    Resets the terminal into sane mode, then frees its associated resources and
    removes it from the terminal list.
*/
defun($h_Terminal_destroy) {
  set_term($$p_Terminal_screen);
  endwin();
  delscreen($$p_Terminal_screen);
  $lo_terminals = lrm_o($lo_terminals, $o_Terminal);

  $f_Consumer_destroy();
  fclose($p_Terminal_input);
  fclose($p_Terminal_output);
}

/*
  SYMBOL: $h_Terminal_read
    Reads characters from the Terminal until no more are available without
    blocking.

  SYMBOL: $i_Terminal_input_type
    The value returned by get_wch (see get_wch(3ncurses)) on the most recent
    call (ie, in $f_Terminal_read).

  SYMBOL: $i_Terminal_input_value
    The character value read in the most recent call to get_wch (ie, in
    $f_Terminal_read). If $i_Terminal_input_type is 0, this is the Unicode
    value of the character the user typed.

  SYMBOL: $f_Terminal_getch
    Called for each character read from the Terminal, within the Terminal's
    context.
*/
defun($h_Terminal_read) {
  set_term($$p_Terminal_screen);
  while (-1 != ($i_Terminal_input_type = get_wch(&$i_Terminal_input_value))) {
    $f_Terminal_getch();
  }
  //TODO: EOF
}

advise_after($h_graceful_exit) {
  if ($y_is_handling_signal && !$y_signal_is_synchronous)
    return;
  void f(object o) {
    $M_destroy(0, o);
  }
  each_o($lo_terminals, f);
}

advise_after($h_die_gracelessly) {
  if ($y_is_handling_signal && !$y_signal_is_synchronous)
    return;
  void f(object o) {
    $M_destroy(0, o);
  }
  each_o($lo_terminals, f);
}

