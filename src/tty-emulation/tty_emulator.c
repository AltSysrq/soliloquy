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
#include "tty_emulator.slc"
#include "../face.h"

/*
  TITLE: Basic TTY Emulator
  OVERVIEW: Provides the very basic infastructure for tty emulation, excluding
    control sequences and such.
 */

/*
  SYMBOL: $c_TtyEmulator
    Encapsulates the data and operations for a primitive terminal emulator.

  SYMBOL: $aax_TtyEmulator_screen
    The current contents of the TtyEmulator. It is not necessarily a
    rectangular array. The outer array contains the rows, and must have a
    length of at least one. The initial value has one row whose size is
    $i_column_width.

  SYMBOL: $ay_TtyEmulator_dirty
    Tracks which rows of $aax_TtyEmulator_screen are dirty; that is, those that
    have been modified since the last call to $m_update(). Its length must be
    the same as that of $aax_TtyEmulator_screen.

  SYMBOL: $I_TtyEmulator_x $I_TtyEmulator_y
    The coordinates within $aax_TtyEmulator_screen of the next character to be
    output.

  SYMBOL: $I_TtyEmulator_ninputs
    The number of TtyConsumers providing inputs to this TtyEmulator.
 */
defun($h_TtyEmulator) {
  $aax_TtyEmulator_screen = dynar_new_ax();
  dynar_expand_by_ax($aax_TtyEmulator_screen, 1);
  $aax_TtyEmulator_screen->v[0] = dynar_new_x();
  dynar_expand_by_x($aax_TtyEmulator_screen->v[0], $i_column_width);
  $ay_TtyEmulator_dirty = dynar_new_y();
  dynar_expand_by_y($ay_TtyEmulator_dirty, $aax_TtyEmulator_screen->len);
}

/*
  SYMBOL: $f_TtyEmulator_addch
    Adds the character $z_TtyEmulator_wch to the output. The default places the
    character at the cursor and advances the cursor if it is not a control
    character, or calls $m_control_character() otherwise.

  SYMBOL: $f_TtyEmulator_control_character
    Called by $f_TtyEmulator_addch when $z_TtyEmulator_wch is a control
    character. The default does nothing.

  SYMBOL: $z_TtyEmulator_wch
    The wchar_t to add in a call to $f_TtyEmulator_addch and
    $f_TtyEmulator_control_character.

  SYMBOL: $I_TtyEmulator_current_face
    The current face for new characters output to this TtyEmulator.
 */
defun($h_TtyEmulator_addch) {
  if ($z_TtyEmulator_wch >= L' ' && $z_TtyEmulator_wch != 127) {
    $aax_TtyEmulator_screen->v[$I_TtyEmulator_y]->v[$I_TtyEmulator_x++] =
      apply_face($I_TtyEmulator_current_face, $z_TtyEmulator_wch);

    $ay_TtyEmulator_dirty->v[$I_TtyEmulator_y] = true;

    if ($I_TtyEmulator_x==$aax_TtyEmulator_screen->v[$I_TtyEmulator_y]->len) {
      // Hit end-of-line
      if ($I_TtyEmulator_y == $aax_TtyEmulator_screen->len)
        $m_scroll();
      else
        ++$I_TtyEmulator_y;

      $I_TtyEmulator_x = 0;
    }
  } else {
    $m_control_character();
  }
}

/*
  SYMBOL: $f_TtyEmulator_scroll
    Called to scroll the TTY down one line. The default moves all lines (but
    the zeroth) up one, and resets the bottom-most line to a dynar of NUL chars
    whose length is $i_column_width.
 */ 
defun($h_TtyEmulator_scroll) {
  memmove($aax_TtyEmulator_screen->v,
          $aax_TtyEmulator_screen->v+1,
          sizeof(dynar_ax*)*$aax_TtyEmulator_screen->len-1);
  $aax_TtyEmulator_screen->v[$aax_TtyEmulator_screen->len-1] =
    dynar_new_x();
  dynar_expand_by_x(
    $aax_TtyEmulator_screen->v[$aax_TtyEmulator_screen->len-1],
    $i_column_width);
}

/*
  SYMBOL: $f_TtyEmulator_update
    Called by the TtyConsumer after current input has been exhausted. This
    method should actually update whatever display the TtyEmulator maps to. The
    default cleans the dirty bits in the AFTER priority.
 */
advise_id_after($u_main, $h_TtyEmulator_update) {
  memset($ay_TtyEmulator_dirty->v, 0, $ay_TtyEmulator_dirty->len*sizeof(bool));
}

/*
  SYMBOL: $f_TtyEmulator_release
    Called by the destruction of a TtyConsumer to notify the TtyEmulator that
    it has one fewer input. Decrements $I_TtyEmulator_ninputs; if it hits zero,
    calls $m_destroy().

  SYMBOL: $f_TtyEmulator_destroy
    Called when the TtyEmulator's input count becomes zero. The default does
    nothing.
 */
defun($h_TtyEmulator_release) {
  if (!--$I_TtyEmulator_ninputs)
    $m_destroy();
}
