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
#include "basic_control_chars.slc"

/*
  TITLE: TTY Emulation of Basic Control Characters
  OVERVIEW: Extends the basic TtyEmulator to understand some ASCII control
    characters, such as line-feed and carraige-return.
 */

/*
  SYMBOL: $u_line_feed_support
    Identifies the hook on $h_TtyEmulator_control_character which adds support
    for the line feed (\n) character. A line feed causes the cursor X to be set
    to zero, and the Y incremented if not at the bottom, or the screen scrolled
    if already at the bottom.
 */
advise_id($u_line_feed_support, $h_TtyEmulator_control_character) {
  if ($z_TtyEmulator_wch == L'\n') {
    $I_TtyEmulator_x = 0;
    if ($I_TtyEmulator_y == $aaz_TtyEmulator_screen->len)
      $m_scroll();
    else
      ++$I_TtyEmulator_y;
  }
}

/*
  SYMBOL: $u_carraige_return_support
    Identifies the hook on $h_TtyEmulator_control_character which adds support
    for the carraige return (\r) character. A carraige return sets the cursor X
    to zero.
 */
advise_id($u_carraige_return_support, $h_TtyEmulator_control_character) {
  if ($z_TtyEmulator_wch == L'\r') {
    $I_TtyEmulator_x = 0;
  }
}

/*
  SYMBOL: $u_form_feed_support
    Identifies the hook on $h_TtyEmulator_control_character which adds support
    for the form feed (\f) character. A form feed calls $m_scroll() a number of
    times equal to the number of rows in $aaz_TtyEmulator_screen, then resets
    the cursor X and Y to the origin.
 */
advise_id($u_form_feed_support, $h_TtyEmulator_control_character) {
  if ($z_TtyEmulator_wch == L'\f') {
    for (unsigned i = 0; i < $aaz_TtyEmulator_screen->len; ++i)
      $m_scroll();
    $I_TtyEmulator_y = 0;
    $I_TtyEmulator_x = 0;
  }
}

/*
  SYMBOL: $u_backspace_support
    Identifies the hook on $h_TtyEmulator_control_character which adds support
    for the backspace (\b) character. A backspace decrements the X value of the
    cursor if it is not already zero.
 */
advise_id($u_backspace_support, $h_TtyEmulator_control_character) {
  if ($z_TtyEmulator_wch == L'\b') {
    if ($I_TtyEmulator_x)
      --$I_TtyEmulator_x;
  }
}
