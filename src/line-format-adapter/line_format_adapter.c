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
#include "line_format_adapter.slc"

/*
  TITLE: Adapter for line-formatting functions
  OVERVIEW: Adapts the interface of BufferEditor and LineEditor to support
    reformatting the text they display, including the repositioning of the
    cursor, via a simple two-hook interface.
 */

/*
  SYMBOL: $u_line_format_adapter
    Identifies the hooks inserted by the line format adapter.

  SYMBOL: $f_line_format_check
    Hooks should examine the string in $q_line_format to determine whether
    there is any reason for them to alter it. If there is, they must set
    $y_line_format_change to true; otherwise, they must leave it alone. If the
    size of the string is to be altered, $I_line_format_size should be altered
    appropriately, by adjusting it by relative size. Hooks may not assume that
    $I_line_format_size reflects the actual length of the string -- it may have
    been altered by other hooks. Generally, shrinking the string is a bad idea,
    since hooks could interact poorly (eg, reducing the size below zero).

  SYMBOL: $f_line_format_move
    Performs in-place formatting adjustments on the string in
    $Q_line_format. Hooks may change the size of the string; if they do so,
    they must maintain $I_line_format_point such that it remains indexing the
    same logical character.

  SYMBOL: $q_line_format
    Input for string $f_line_format_check().

  SYMBOL: $Q_line_format
    Input/output string for $f_line_format_move(). When that function is
    called, it has already been initialised.

  SYMBOL: $Q_line_format_back
    Set when $f_line_format_move() is called if
    $y_line_format_needs_back_buffer was true. It is a buffer of equal size to
    $Q_line_format, but initially uninitialised. Operations which would be able
    to execute more efficiently by copying from one buffer to another may want
    this; if they do, they should first swap $Q_line_format and
    $Q_line_format_back, then copy from the latter to the former.

  SYMBOL: $I_line_format_size
    The size of the final string to be produced by $f_line_format_move(). See
    documentation on $f_line_format_check() and $f_line_format_move() for
    details.

  SYMBOL: $I_line_format_point
    The index of the user's point within $Q_line_format, which must be
    maintained if characters within are rearranged.

  SYMBOL: $y_line_format_change
    If set to true by a hook on $f_line_format_check(), the string in question
    will be replaced, and $f_line_format_move() will be called.

  SYMBOL: $y_line_format_needs_back_buffer
    If true, $Q_line_format_back is initialised before calling
    $f_line_format_move().
 */

advise_id_after($u_line_format_adapter, $h_LineEditor_get_echo_area_contents) {
  if ($u_echo_off == ($v_LineEditor_echo_mode ?: $v_Workspace_echo_mode))
    // Nothing to change
    return;

  $q_line_format = $q_Workspace_echo_area_contents;
  $I_line_format_size = qstrlen($q_Workspace_echo_area_contents);
  $I_line_format_point = ($i_LineEditor_point != -1? $i_LineEditor_point : 0);
  $y_line_format_change = false;
  $y_line_format_needs_back_buffer = false;
  $f_line_format_check();

  if ($y_line_format_change) {
    $Q_line_format = qcalloc($I_line_format_size+1);
    if ($y_line_format_needs_back_buffer)
      $Q_line_format_back = qcalloc($I_line_format_size+1);
    qstrlcpy($Q_line_format, $q_line_format, $I_line_format_size+1);
    $f_line_format_move();
    $q_Workspace_echo_area_contents = $Q_line_format;

    if ($i_LineEditor_point != -1)
      $i_LineEditor_point = $I_line_format_point;
  }
}

advise_id_after($u_line_format_adapter, $h_BufferEditor_prettify) {
  $q_line_format = $q_RenderedLine_body;
  $I_line_format_size = qstrlen($q_RenderedLine_body);
  $I_line_format_point = 0;
  $y_line_format_change = false;
  $y_line_format_needs_back_buffer = false;
  $f_line_format_check();

  if ($y_line_format_change) {
    $Q_line_format = qcalloc($I_line_format_size+1);
    if ($y_line_format_needs_back_buffer)
      $Q_line_format_back = qcalloc($I_line_format_size+1);
    qstrlcpy($Q_line_format, $q_line_format, $I_line_format_size+1);
    $f_line_format_move();
    $q_RenderedLine_body = $Q_line_format;
  }
}
