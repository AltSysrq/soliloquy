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
#include "line_editor.slc"
#include <time.h>
#include <ctype.h>
#include <wctype.h>
#include "key_dispatch.h"
#include "interactive.h"

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
STATIC_INIT_TO($v_Workspace_echo_mode, $u_echo_on)

/*
  SYMBOL: $i_Line_Editor_cursor
    The current insert position within the Line_Editor's buffer. If it is -1
    when the Line_Editor is constructed, it is set to the length of the initial
    buffer.

  SYMBOL: $w_Line_Editor_text
    If non-NULL, the initial text for the Line_Editor when it is
    constructed. Otherwise, the initial text is the empty string.

  SYMBOL: $az_Line_Editor_buffer
    An array of integers (wchar_ts) which comprise the current text of the
    Line_Editor.

  SYMBOL: $v_Line_Editor_echo_mode
    The echo mode specific to this Line_Editor. If NULL, it is inherited from
    $v_Workspace_echo_mode.
 */
STATIC_INIT_TO($i_Line_Editor_cursor, -1)

defun($h_Line_Editor) {
  if (!$w_Line_Editor_text)
    $az_Line_Editor_buffer = dynar_new_z();
  else {
    $az_Line_Editor_buffer = dynar_new_z();
    dynar_expand_by_z($az_Line_Editor_buffer, wcslen($w_Line_Editor_text));
    for (unsigned i = 0; $w_Line_Editor_text[i]; ++i)
      $az_Line_Editor_buffer->v[i] = (signed)(unsigned)$w_Line_Editor_text[i];
  }

  if (-1 == $i_Line_Editor_cursor)
    $i_Line_Editor_cursor = $az_Line_Editor_buffer->len;
}

/*
  SYMBOL: $f_Line_Editor_push_undo
    Pushes a copy of the current edit buffer onto the undo stack, and clears
    the redo stack. The current top of the undo stack will be preserved if
    $y_Line_Editor_edit_is_minor is true,
    $y_Line_Editor_previous_edit_was_minor is true, and
    $i_Line_Editor_last_edit is still equal to time(0).

  SYMBOL: $laz_Line_Editor_undo
    A list of copies of former values of $az_Line_Editor_buffer, representing
    states that the user can undo back to.

  SYMBOL: $laz_Line_Editor_redo
    A list of copies of former values of $az_Line_Editor_buffer, representing
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
    lpush_az($laz_Line_Editor_undo, dynar_clone_z($az_Line_Editor_buffer));

  $y_Line_Editor_previous_edit_was_minor =
    $y_Line_Editor_edit_is_minor;
  $i_Line_Editor_last_edit = time(0);

  $laz_Line_Editor_redo = NULL;
}

/*
  SYMBOL: $f_Line_Editor_self_insert
    Inserts $x_Terminal_input_value into $az_Line_Editor_buffer at
    $i_Line_Editor_cursor, then increments the cursor position. If
    $x_Terminal_input_value is not a non-control character, the function sets
    $y_key_dispatch_continue to true and returns without taking action.
 */
defun($h_Line_Editor_self_insert) {
  if (!is_nc_char($x_Terminal_input_value)) {
    // Control character or non-character
    $y_key_dispatch_continue = true;
    return;
  }

  let($y_Line_Editor_edit_is_minor, true);
  $m_push_undo();
  wchar_t input = (wchar_t)$x_Terminal_input_value;
  dynar_ins_z($az_Line_Editor_buffer, $i_Line_Editor_cursor++,
              &input, 1);

  $m_changed();
}

/*
  SYMBOL: $f_Line_Editor_changed
    Called after modifications to $az_Line_Editor_buffer have occurred, so that
    the echo area can be updated as needed, etc. This must be called within the
    context of the current Workspace. Besides repainting the echo area, it also
    ensures that the cursor is within allowable boundaries.
 */
defun($h_Line_Editor_changed) {
  if ($i_Line_Editor_cursor < 0)
    $i_Line_Editor_cursor = 0;
  else if ($i_Line_Editor_cursor > $az_Line_Editor_buffer->len)
    $i_Line_Editor_cursor = $az_Line_Editor_buffer->len;

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
  mqstring result = gcalloc((1+$az_Line_Editor_buffer->len) *
                            sizeof(qchar));
  for (unsigned i = 0; i < $az_Line_Editor_buffer->len; ++i)
    result[i] = (qchar)$az_Line_Editor_buffer->v[i];
  result[$az_Line_Editor_buffer->len] = 0;

  $q_Workspace_echo_area_contents = result;

  if ($u_echo_off != ($v_Line_Editor_echo_mode ?: $v_Workspace_echo_mode))
    $i_Workspace_echo_area_cursor = $i_Line_Editor_cursor;
  else
    $i_Workspace_echo_area_cursor = -1;
}

/*
  SYMBOL: $f_Line_Editor_delete_backward_char
    Delete the character immediately before the cursor.
 */
defun($h_Line_Editor_delete_backward_char) {
  if ($i_Line_Editor_cursor != 0) {
    let($y_Line_Editor_edit_is_minor, true);
    $f_Line_Editor_push_undo();
    --$i_Line_Editor_cursor;
    dynar_erase_z($az_Line_Editor_buffer, $i_Line_Editor_cursor, 1);

    $m_changed();
  }
}

/*
  SYMBOL: $f_Line_Editor_delete_forward_char
    Delete the character immediately after the cursor.
 */
defun($h_Line_Editor_delete_forward_char) {
  if ($i_Line_Editor_cursor != $az_Line_Editor_buffer->len) {
    let($y_Line_Editor_edit_is_minor, true);
    $f_Line_Editor_push_undo();
    dynar_erase_z($az_Line_Editor_buffer, $i_Line_Editor_cursor, 1);

    $m_changed();
  }
}

/*
  SYMBOL: $f_Line_Editor_move_forward_char
    Moves the cursor one character to the right.
 */
defun($h_Line_Editor_move_forward_char) {
  if ($i_Line_Editor_cursor != $az_Line_Editor_buffer->len) {
    ++$i_Line_Editor_cursor;
    $m_changed();
  }
}

/*
  SYMBOL: $f_Line_Editor_move_backward_char
    Moves the cursor one character to the left.
 */
defun($h_Line_Editor_move_backward_char) {
  if ($i_Line_Editor_cursor != 0) {
    --$i_Line_Editor_cursor;
    $m_changed();
  }
}

/*
  SYMBOL: $f_Line_Editor_move_forward_word
    Moves the cursor forward one word, as defined by is_word_boundary().
 */
defun($h_Line_Editor_move_forward_word) {
  if ($i_Line_Editor_cursor == $az_Line_Editor_buffer->len)
    // Already at end
    return;

  do {
    ++$i_Line_Editor_cursor;
  } while ($i_Line_Editor_cursor != $az_Line_Editor_buffer->len &&
           !is_word_boundary(
             $az_Line_Editor_buffer->v[$i_Line_Editor_cursor-1],
             $az_Line_Editor_buffer->v[$i_Line_Editor_cursor  ]));

  $m_changed();
}

/*
  SYMBOL: $f_Line_Editor_move_backward_word
    Moves the cursor backward one word, as defined by is_word_boundary().
 */
defun($h_Line_Editor_move_backward_word) {
  if ($i_Line_Editor_cursor == 0)
    // Already at the beginning
    return;

  do {
    --$i_Line_Editor_cursor;
  } while ($i_Line_Editor_cursor &&
           !is_word_boundary(
             $az_Line_Editor_buffer->v[$i_Line_Editor_cursor-1],
             $az_Line_Editor_buffer->v[$i_Line_Editor_cursor  ]));

  $m_changed();
}

/*
  SYMBOL: $f_Line_Editor_home
    Moves cursor to the first non-whitespace character, or to column zero if it
    was already there.
 */
defun($h_Line_Editor_home) {
  unsigned firstNonWhitespace = 0;
  while (firstNonWhitespace < $az_Line_Editor_buffer->len &&
         iswspace($az_Line_Editor_buffer->v[firstNonWhitespace]))
    ++firstNonWhitespace;

  if ($i_Line_Editor_cursor == firstNonWhitespace)
    $i_Line_Editor_cursor = 0;
  else
    $i_Line_Editor_cursor = firstNonWhitespace;

  $m_changed();
}

/*
  SYMBOL: $f_Line_Editor_end
    Moves the cursor past the last character in the line.
 */
defun($h_Line_Editor_end) {
  $i_Line_Editor_cursor = $az_Line_Editor_buffer->len;
  $m_changed();
}

/*
  SYMBOL: $h_Line_Editor_seek_forward_to_char $h_Line_Editor_seek_forward_to_char_i
    Moves point forward by characters until the end of the buffer or
    $z_Line_Editor_seek_dst is encountered.

  SYMBOL: $z_Line_Editor_seek_dst
    Character to which to seek on target-based functions.
 */
interactive($h_Line_Editor_seek_forward_to_char_i,
            $h_Line_Editor_seek_forward_to_char,
            i_(z, $z_Line_Editor_seek_dst, L"Seek")) {
  while ($i_Line_Editor_cursor < $az_Line_Editor_buffer->len &&
         $z_Line_Editor_seek_dst !=
           $az_Line_Editor_buffer->v[$i_Line_Editor_cursor])
    ++$i_Line_Editor_cursor;
  $m_changed();
}

/*
  SYMBOL: $h_Line_Editor_seek_backward_to_char $h_Line_Editor_seek_backward_to_char_i
    Moves point backward by characters until the beginning of the buffer or
    $z_Line_Editor_seek_dst is encountered.
 */
interactive($h_Line_Editor_seek_backward_to_char_i,
            $h_Line_Editor_seek_backward_to_char,
            i_(z, $z_Line_Editor_seek_dst, L"Seek")) {
  while ($i_Line_Editor_cursor > 0 &&
         $z_Line_Editor_seek_dst !=
           $az_Line_Editor_buffer->v[$i_Line_Editor_cursor])
    --$i_Line_Editor_cursor;
  $m_changed();
}

/*
  SYMBOL: $lp_Line_Editor_keybindings
    List of keybindings supported by generic Line_Editors.
 */
class_keymap($c_Line_Editor, $lp_Line_Editor_keybindings, $llp_Activity_keymap)
ATSTART(setup_line_editor_keybindings, STATIC_INITIALISATION_PRIORITY) {
  bind_kp($lp_Line_Editor_keybindings, $u_ground, KEYBINDING_DEFAULT, NULL,
          $f_Line_Editor_self_insert);
  bind_char($lp_Line_Editor_keybindings, $u_meta, L'j', $v_end_meta,
            $f_Line_Editor_move_backward_char);
  bind_char($lp_Line_Editor_keybindings, $u_meta, L'k', $v_end_meta,
            $f_Line_Editor_move_forward_char);
  bind_char($lp_Line_Editor_keybindings, $u_meta, L'u', $v_end_meta,
            $f_Line_Editor_move_backward_word);
  bind_char($lp_Line_Editor_keybindings, $u_meta, L'i', $v_end_meta,
            $f_Line_Editor_move_forward_word);
  bind_char($lp_Line_Editor_keybindings, $u_meta, L'J', $v_end_meta,
            $f_Line_Editor_seek_backward_to_char_i);
  bind_char($lp_Line_Editor_keybindings, $u_meta, L'K', $v_end_meta,
            $f_Line_Editor_seek_forward_to_char_i);
  bind_char($lp_Line_Editor_keybindings, $u_meta, L'l', $v_end_meta,
            $f_Line_Editor_delete_backward_char);
  bind_char($lp_Line_Editor_keybindings, $u_meta, L';', $v_end_meta,
            $f_Line_Editor_delete_forward_char);
  bind_char($lp_Line_Editor_keybindings, $u_meta, L'h', $v_end_meta,
            $f_Line_Editor_home);
  bind_char($lp_Line_Editor_keybindings, $u_meta, L'n', $v_end_meta,
            $f_Line_Editor_end);
}
