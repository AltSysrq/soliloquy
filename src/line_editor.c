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
#include <time.h>
#include "line_editor.slc"
#include "key_dispatch.h"

/*
  TITLE: Line Editor Abstract Class
  OVERVIEW: Provides the abstract class $c_Line_Editor, which allows the user
    to edit a single line of text interactively. Also defines the default
    keybindings for line editing.
 */

/*
  SYMBOL: $c_Line_Editor
    An Activity which allows the user to edit a single line of text.
 */
subclass($c_Activity, $c_Line_Editor)

/*
  SYMBOL: $v_Workspace_echo_mode
    The default echo mode for new Line_Editors within the Workspace.
    Should be one of $u_echo_on, $u_echo_off, or $u_echo_ghost.

  SYMBOL: $u_echo_on
    Echo mode in which the user's text and cursor are shown in the echo area.

  SYMBOL: $u_echo_off
    Echo mode in which the user's text and cursor are hidden from the echo
    area.

  SYMBOL: $u_echo_ghost
    Echo mode in which the user's text is hidden, but cursor is shown, in the
    ecno area.
 */
STATIC_INIT_TO($v_Workspace_echo_mode, $u_echo_ghost)

/*
  SYMBOL: $i_Line_Editor_cursor
    The current insert position within the Line_Editor's buffer. If it is -1
    when the Line_Editor is constructed, it is set to the length of the initial
    buffer.

  SYMBOL: $w_Line_Editor_text
    If non-NULL, the initial text for the Line_Editor when it is
    constructed. Otherwise, the initial text is the empty string.

  SYMBOL: $ai_Line_Editor_buffer
    An array of integers (wchar_ts) which comprise the current text of the
    Line_Editor.

  SYMBOL: $v_Line_Editor_echo_mode
    The echo mode specific to this Line_Editor. If NULL, it is inherited from
    $v_Workspace_echo_mode.
 */
STATIC_INIT_TO($i_Line_Editor_cursor, -1)

defun($h_Line_Editor) {
  if (!$w_Line_Editor_text)
    $ai_Line_Editor_buffer = dynar_new_i();
  else {
    $ai_Line_Editor_buffer = dynar_new_i();
    dynar_expand_by_i($ai_Line_Editor_buffer, wcslen($w_Line_Editor_text));
    for (unsigned i = 0; $w_Line_Editor_text[i]; ++i)
      $ai_Line_Editor_buffer->v[i] = (signed)(unsigned)$w_Line_Editor_text[i];
  }

  if (-1 == $i_Line_Editor_cursor)
    $i_Line_Editor_cursor = $ai_Line_Editor_buffer->len;
}

/*
  SYMBOL: $f_Line_Editor_push_undo
    Pushes a copy of the current edit buffer onto the undo stack, and clears
    the redo stack. The current top of the undo stack will be preserved if
    $y_Line_Editor_edit_is_minor is true,
    $y_Line_Editor_previous_edit_was_minor is true, and
    $i_Line_Editor_last_edit is still equal to time(0).

  SYMBOL: $lai_Line_Editor_undo
    A list of copies of former values of $ai_Line_Editor_buffer, representing
    states that the user can undo back to.

  SYMBOL: $lai_Line_Editor_redo
    A list of copies of former values of $ai_Line_Editor_buffer, representing
    states that the user moved away from by using undo.

  SYMBOL: $y_Line_Editor_edit_is_minor
    Indicates whether the current edit to the Line_Editor buffer is
    "minor". Consecutive minor edits made in short succession will not push
    multiple states onto the undo stack.

  SYMBOL: $y_Line_Editor_previous_edit_was_minor
    Indicates whether the previous call to $f_Line_Editor_push_undo was due to
    a minor edit.

  SYMBOL: $i_Line_Editor_last_edit
    The time(0) of the most recent call to $f_Line_Editor_push_undo.
 */
defun($h_Line_Editor_push_undo) {
  if (!$y_Line_Editor_edit_is_minor ||
      !$y_Line_Editor_previous_edit_was_minor ||
      time(0) != $i_Line_Editor_last_edit)
    lpush_ai($lai_Line_Editor_undo, dynar_clone_i($ai_Line_Editor_buffer));

  $y_Line_Editor_previous_edit_was_minor =
    $y_Line_Editor_edit_is_minor;
  $i_Line_Editor_last_edit = time(0);

  $lai_Line_Editor_redo = NULL;
}

/*
  SYMBOL: $f_Line_Editor_self_insert
    Inserts $i_Terminal_input_value into $ai_Line_Editor_buffer at
    $i_Line_Editor_cursor, then increments the cursor position. If
    $i_Terminal_input_value is not a non-control character, the function sets
    $y_key_dispatch_continue to true and returns without taking action.
 */
defun($h_Line_Editor_self_insert) {
  if (((unsigned)$i_Terminal_input_value) < L' ' ||
      $i_Terminal_input_value == 0x7F ||
      ($i_Terminal_input_value & (1<<31))) {
    // Control character or non-character
    $y_key_dispatch_continue = true;
    return;
  }

  let($y_Line_Editor_edit_is_minor, true);
  $m_push_undo();
  dynar_ins_i($ai_Line_Editor_buffer, $i_Line_Editor_cursor++,
              &$i_Terminal_input_value, 1);

  $m_changed();
}

/*
  SYMBOL: $f_Line_Editor_changed
    Called after modifications to $ai_Line_Editor_buffer have occurred, so that
    the echo area can be updated as needed, etc. This must be called within the
    context of the current Workspace.
 */
defun($h_Line_Editor_changed) {
  $f_Workspace_update_echo_area();
}

/*
  SYMBOL: $f_Line_Editor_is_echo_enabled
    Sets $y_Workspace_is_echo_enabled to indicate whether the echo area should
    show the line contents.
 */
defun($h_Line_Editor_is_echo_enabled) {
  identity mode = $v_Line_Editor_echo_mode ?: $v_Workspace_echo_mode;
  $y_Workspace_is_echo_enabled = (mode == $u_echo_on);
}

/*
  SYMBOL: $f_Line_Editor_get_echo_area_contents
    Converts the Line_Editor buffer into an unformatted qstring and sets the
    cursor position therein (see $m_get_echo_area_contents).
 */
defun($h_Line_Editor_get_echo_area_contents) {
  mqstring result = gcalloc((1+$ai_Line_Editor_buffer->len) *
                            sizeof(qchar));
  for (unsigned i = 0; i < $ai_Line_Editor_buffer->len; ++i)
    result[i] = (qchar)$ai_Line_Editor_buffer->v[i];
  result[$ai_Line_Editor_buffer->len] = 0;

  $q_Workspace_echo_area_contents = result;

  if ($u_echo_off != ($v_Line_Editor_echo_mode ?: $v_Workspace_echo_mode))
    $i_Workspace_echo_area_cursor = $i_Line_Editor_cursor;
  else
    $i_Workspace_echo_area_cursor = -1;
}

/*
  SYMBOL: $lp_Line_Editor_keybindings
    List of keybindings supported by generic Line_Editors.
 */
class_keymap($c_Line_Editor, $lp_Line_Editor_keybindings, $llp_Activity_keymap)
ATSTART(setup_line_editor_keybindings, STATIC_INITIALISATION_PRIORITY) {
  bind_kp($lp_Line_Editor_keybindings, $u_ground, KEYBINDING_DEFAULT, NULL,
          $f_Line_Editor_self_insert);
}
