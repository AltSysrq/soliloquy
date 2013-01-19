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
#include "echo_area.slc"

/*
  TITLE: Echo Area Handling
  OVERVIEW: Manages the echo area (the bottom-most line of the terminal).
 */

/*
  SYMBOL: $f_Workspace_draw_echo_area
    Draws the echo area for the active Workspace. This must be called within
    the context of the Terminal and View on which to draw.

  SYMBOL: $q_Workspace_echo_area_contents
    The primary contents of the echo area in the current Workspace. This should
    be set by calls to $m_get_echo_area_contents() on the current Activity.

  SYMBOL: $q_Workspace_echo_area_meta
    The metadata to display in the echo area of the current Workspace. This
    should be set by calls to $m_get_echo_area_contents() on the current
    Activity.

  SYMBOL: $i_Workspace_echo_area_cursor
    The location of the logical cursor within the contents of the echo area on
    this Workspace, or -1 for no cursor. This should be set by calls to
    $m_get_echo_area_contents() on the current Activity.

  SYMBOL: $m_get_echo_area_contents
    Method on Activity to set the current echo area details
    ($q_Workspace_echo_area_contents, $q_Workspace_echo_area_meta, and
    $i_Workspace_echo_area_cursor) to be drawn on the current
    View/Terminal. When this is called, both contents and meta are empty
    strings, and the cursor is -1 (invisible).

  SYMBOL: $m_is_echo_enabled
    Method on Activity to set $y_Workspace_is_echo_enabled to indicate whether
    the current Activity wants the user to see the contents of the echo area.

  SYMBOL: $y_Workspace_is_echo_enabled
    If true, the contents of the echo area will be displayed to the user. If
    false, only metadata will be displayed. In either case, the position of the
    cursor will be maintained on the screen.

  SYMBOL: $i_View_echo_area_scroll
    The index one beyond the last character in the echo area to draw, or the
    width of the terminal, whichever is greater. Drawing the echo area will
    automatically maintain this to ensure that the cursor is visible.
 */
defun($h_Workspace_draw_echo_area) {
  // Set up the default echo area
  $q_Workspace_echo_area_contents = qempty;
  $q_Workspace_echo_area_meta = qempty;
  $i_Workspace_echo_area_cursor = -1;
  // Get actual contents
  if ($lo_Workspace_activities)
    $M_get_echo_area_contents(
      0, $lo_Workspace_activities->car);

  qchar str[$i_Terminal_cols+1];
  memset(str, 0, sizeof(str));

  // Try to show as much meta as possible, but give at least $i_column_width
  // characters to the contents.
  // If echo is off, instead devote as much as possible to metadata.
  int contents_size, meta_size;
  if ($lo_Workspace_activities &&
      $M_is_echo_enabled($y_Workspace_is_echo_enabled,
                         $lo_Workspace_activities->car)) {
    contents_size = $i_Terminal_cols - qstrlen($q_Workspace_echo_area_meta);
    if (contents_size < $i_column_width)
      contents_size = $i_column_width;
    meta_size = $i_Terminal_cols - contents_size;
  } else {
    contents_size = 0;
    meta_size = qstrlen($q_Workspace_echo_area_meta);
    if (meta_size > $i_Terminal_cols)
        meta_size = $i_Terminal_cols;
  }

  // Ensure that the cursor is visible (if present), and that the scrolling is
  // sane.
  if ($i_View_echo_area_scroll > qstrlen($q_Workspace_echo_area_contents) ||
      $i_View_echo_area_scroll < contents_size)
    $i_View_echo_area_scroll = contents_size;

  if ($i_Workspace_echo_area_cursor != -1) {
    if ($i_Workspace_echo_area_cursor + contents_size <
        $i_View_echo_area_scroll)
      $i_View_echo_area_scroll = $i_Workspace_echo_area_cursor + contents_size;
    else if ($i_Workspace_echo_area_cursor >= $i_View_echo_area_scroll)
      $i_View_echo_area_scroll = $i_Workspace_echo_area_cursor + 1;
  }

  // Populate contents of str
  qstrlcpy(str, $q_Workspace_echo_area_contents, contents_size+1);
  qstrlcpy(str + $i_Terminal_cols - meta_size, $q_Workspace_echo_area_meta,
           meta_size+1);

  //Set cursor status and render
  if (-1 != $i_Workspace_echo_area_cursor) {
    $i_Terminal_cursor_x =
      $i_Workspace_echo_area_cursor + contents_size - $i_View_echo_area_scroll;
    $i_Terminal_cursor_y = $i_Terminal_rows - 1;
    $y_Terminal_cursor_visible = true;
  } else {
    $y_Terminal_cursor_visible = false;
  }
  let($i_y, $i_Terminal_rows - 1);
  for (unsigned i = 0; i < $i_Terminal_cols; ++i) {
    let($i_x, i);
    let($q_qch, str + i);
    $m_putch();
  }
}
