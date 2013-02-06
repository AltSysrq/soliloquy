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
  OVERVIEW: Manages the connect terminals, including the ncurses interface.
*/

#include "terminal.slc"

#include "inc_ncurses.h"

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

  SYMBOL: $i_Terminal_rows $i_Terminal_cols
    The current number of rows (lines) or columns present on the Terminal.

 */

subclass($c_Consumer,$c_Terminal)
member_of_domain($$d_Terminal, $d_Terminal)

STATIC_INIT_TO($y_Terminal_cursor_visible, true)
STATIC_INIT_TO($$y_Terminal_cursor_visible, true)

defun($h_Terminal) {
  $$p_Terminal_screen = newterm($s_Terminal_type,
                                $p_Terminal_output, $p_Terminal_input);
  $i_Consumer_fd = fileno((FILE*)$p_Terminal_input);

  if (!$$p_Terminal_screen) {
    $y_Terminal_ok = false;
    // Remove from consumers since we're in an invalid state
    $f_Consumer_destroy();
    return;
  }

  $i_Terminal_rows = LINES;
  $i_Terminal_cols = COLS;

  $y_Terminal_ok = true;
  $f_Terminal_enter_raw_mode();
  $$y_Terminal_needs_refresh = false;

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

  del_hook(&$h_kernel_cycle, HOOK_BEFORE, $u_Terminal_refresh, $o_Terminal);
}

/*
  SYMBOL: $h_Terminal_read
    Reads characters from the Terminal until no more are available without
    blocking.

  SYMBOL: $x_Terminal_input_value
    The character value read in the most recent call to get_wch (ie, in
    $f_Terminal_read). If bit 31 of this value is zero, it is a single Unicode
    character or ASCII control character. If bit 31 is set, the lower 31 bits
    are the value of a KEY_ constant returned by curses. (See getch(3ncurses).)

  SYMBOL: $f_Terminal_getch
    Called for each character read from the Terminal, within the Terminal's
    context.
*/
defun($h_Terminal_read) {
  set_term($$p_Terminal_screen);
  wint_t wchar;
  int type;
  while (ERR != (type = get_wch(&wchar))) {
    if (wchar == EOF) {
      $f_Terminal_destroy();
      return;
    }

    $x_Terminal_input_value = wchar;
    if (type == KEY_CODE_YES)
      $x_Terminal_input_value |= (1 << 31);
    $f_Terminal_getch();
  }
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

/*
  SYMBOL: $f_Terminal_putch
    Writes the value of *$q_qch to the terminal at coordinates ($i_x,$i_y),
    where (0,0) is the top-left of the screen. The terminal will automatically
    refresh before the next kernel cycle.

  SYMBOL: $f_translate_qchar_to_ncurses
    Used by $f_Terminal_putch to translate qchars to their most equivalent
    representation in ncurses. It is called once per character that needs
    updating. The function is to read from the first character of $q_qch and
    write into the ncurses cchar_t pointed to by $p_wch.

  SYMBOL: $q_qch $p_wch
    Used as input and output arguments to $f_translate_qchar_to_ncurses,
    respectively. Only the first character of $q_qch is relevant. $p_wch points
    to a stack-allocated cchar_t, and has undefined value outside of a call to
    $f_translate_qchar_to_ncurses.

  SYMBOL: $i_x $i_y
    Coordinates for $f_Terminal_putch.

  SYMBOL: $u_Terminal_refresh
    Identifies the hook used to refresh the terminal.
 */
defun($h_Terminal_putch) {
  set_term($$p_Terminal_screen);

  cchar_t wch;
  let($p_wch, &wch);
  $f_translate_qchar_to_ncurses();
  mvadd_wch($i_y, $i_x, &wch);

  // Schedule refresh if not already so scheduled
  $f_Terminal_update();
}

/*
  SYMBOL: $i_Terminal_cursor_x $i_Terminal_cursor_y
    The coordinates on the screen where the hardware cursor should be
    displayed. If you change these, make sure to call $f_Terminal_update().

  SYMBOL: $y_Terminal_cursor_visible
    Indicates whether the hardware cursor for the Terminal should be
    visible. Changes will not have effect unless $f_Terminal_update() is
    called.

  SYMBOL: $f_Terminal_update
    Schedules a refresh of the Terminal immediately before the next kernel
    cycle begins. This MUST be called to update the cursor location. It is
    called automatically by $f_Terminal_putch(), so calling it only in cases of
    screen display changes is unnecessary.
 */
defun($h_Terminal_update) {
  if (!$$y_Terminal_needs_refresh) {
    $$y_Terminal_needs_refresh = true;
    add_hook_obj(&$h_kernel_cycle, HOOK_BEFORE,
                 $u_Terminal_refresh, $u_Terminal,
                 $$f_Terminal_refresh, $o_Terminal, NULL);
  }
}

defun($$h_Terminal_refresh) {
  set_term($$p_Terminal_screen);
  if ($$y_Terminal_cursor_visible != $y_Terminal_cursor_visible) {
    if (ERR == curs_set($y_Terminal_cursor_visible? 1 : 0))
      // The terminal does not support this cursor mode
      $y_Terminal_cursor_visible = $$y_Terminal_cursor_visible;
    leaveok(stdscr, $y_Terminal_cursor_visible? 0 : 1);
    $$y_Terminal_cursor_visible = $y_Terminal_cursor_visible;
  }

  if ($y_Terminal_cursor_visible)
    move($i_Terminal_cursor_y, $i_Terminal_cursor_x);
  refresh();
  $$y_Terminal_needs_refresh = false;
  del_hook(&$h_kernel_cycle, HOOK_BEFORE, $u_Terminal_refresh, $o_Terminal);
}
