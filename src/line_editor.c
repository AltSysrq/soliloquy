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
  OVERVIEW: Provides the abstract class $c_LineEditor, which allows the user
    to edit a single line of text interactively. Also defines the default
    keybindings for line editing.
 */

/*
  SYMBOL: $c_LineEditor
    An Activity which allows the user to edit a single line of text.
 */
subclass($c_Activity, $c_LineEditor)

/*
  SYMBOL: $v_Workspace_echo_mode
    The default echo mode for new LineEditors within the Workspace.
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
  SYMBOL: $i_LineEditor_cursor
    The current insert position within the LineEditor's buffer. If it is -1
    when the LineEditor is constructed, it is set to the length of the initial
    buffer.

  SYMBOL: $w_LineEditor_text
    If non-NULL, the initial text for the LineEditor when it is
    constructed. Otherwise, the initial text is the empty string.

  SYMBOL: $az_LineEditor_buffer
    An array of integers (wchar_ts) which comprise the current text of the
    LineEditor.

  SYMBOL: $v_LineEditor_echo_mode
    The echo mode specific to this LineEditor. If NULL, it is inherited from
    $v_Workspace_echo_mode.
 */
STATIC_INIT_TO($i_LineEditor_cursor, -1)

defun($h_LineEditor) {
  if (!$w_LineEditor_text)
    $az_LineEditor_buffer = dynar_new_z();
  else {
    $az_LineEditor_buffer = dynar_new_z();
    dynar_expand_by_z($az_LineEditor_buffer, wcslen($w_LineEditor_text));
    for (unsigned i = 0; $w_LineEditor_text[i]; ++i)
      $az_LineEditor_buffer->v[i] = $w_LineEditor_text[i];
  }

  if (-1 == $i_LineEditor_cursor)
    $i_LineEditor_cursor = $az_LineEditor_buffer->len;
}

/*
  SYMBOL: $f_LineEditor_destroy
    Forwards call to $f_Activity_destroy.
 */
defun($h_LineEditor_destroy) {
  $f_Activity_destroy();
}

/*
  SYMBOL: $f_LineEditor_push_undo
    Pushes a copy of the current edit buffer onto the undo stack, and clears
    the redo stack. The current top of the undo stack will be preserved if
    $y_LineEditor_edit_is_minor is true,
    $y_LineEditor_previous_edit_was_minor is true, and
    $i_LineEditor_last_edit is still equal to time(0).

  SYMBOL: $laz_LineEditor_undo
    A list of copies of former values of $az_LineEditor_buffer, representing
    states that the user can undo back to.

  SYMBOL: $laz_LineEditor_redo
    A list of copies of former values of $az_LineEditor_buffer, representing
    states that the user moved away from by using undo.

  SYMBOL: $y_LineEditor_edit_is_minor
    Indicates whether the current edit to the LineEditor buffer is
    "minor". Consecutive minor edits made in short succession will not push
    multiple states onto the undo stack.

  SYMBOL: $y_LineEditor_previous_edit_was_minor
    Indicates whether the previous call to $f_LineEditor_push_undo was due to
    a minor edit.

  SYMBOL: $i_LineEditor_last_edit
    The time(0) of the most recent call to $f_LineEditor_push_undo.
 */
defun($h_LineEditor_push_undo) {
  if (!$y_LineEditor_edit_is_minor ||
      !$y_LineEditor_previous_edit_was_minor ||
      time(0) != $i_LineEditor_last_edit)
    lpush_az($laz_LineEditor_undo, dynar_clone_z($az_LineEditor_buffer));

  $y_LineEditor_previous_edit_was_minor =
    $y_LineEditor_edit_is_minor;
  $i_LineEditor_last_edit = time(0);

  $laz_LineEditor_redo = NULL;
}

/*
  SYMBOL: $f_LineEditor_self_insert
    Inserts $x_Terminal_input_value into $az_LineEditor_buffer at
    $i_LineEditor_cursor, then increments the cursor position. If
    $x_Terminal_input_value is not a non-control character, the function sets
    $y_key_dispatch_continue to true and returns without taking action.
 */
defun($h_LineEditor_self_insert) {
  if (!is_nc_char($x_Terminal_input_value)) {
    // Control character or non-character
    $y_key_dispatch_continue = true;
    return;
  }

  let($y_LineEditor_edit_is_minor, true);
  $m_push_undo();
  wchar_t input = (wchar_t)$x_Terminal_input_value;
  dynar_ins_z($az_LineEditor_buffer, $i_LineEditor_cursor++,
              &input, 1);

  $m_changed();
}

/*
  SYMBOL: $f_LineEditor_changed
    Called after modifications to $az_LineEditor_buffer have occurred, so that
    the echo area can be updated as needed, etc. This must be called within the
    context of the current Workspace. Besides repainting the echo area, it also
    ensures that the cursor is within allowable boundaries.
 */
defun($h_LineEditor_changed) {
  if ($i_LineEditor_cursor < 0)
    $i_LineEditor_cursor = 0;
  else if ($i_LineEditor_cursor > $az_LineEditor_buffer->len)
    $i_LineEditor_cursor = $az_LineEditor_buffer->len;

  $f_Workspace_update_echo_area();
}

/*
  SYMBOL: $f_LineEditor_is_echo_enabled
    Sets $y_Workspace_is_echo_enabled to indicate whether the echo area should
    show the line contents.
 */
defun($h_LineEditor_is_echo_enabled) {
  identity mode = $v_LineEditor_echo_mode ?: $v_Workspace_echo_mode;
  $y_Workspace_is_echo_enabled = (mode == $u_echo_on);
}

/*
  SYMBOL: $f_LineEditor_get_echo_area_contents
    Converts the LineEditor buffer into an unformatted qstring and sets the
    cursor position therein (see $m_get_echo_area_contents).
 */
defun($h_LineEditor_get_echo_area_contents) {
  mqstring result = gcalloc((1+$az_LineEditor_buffer->len) *
                            sizeof(qchar));
  for (unsigned i = 0; i < $az_LineEditor_buffer->len; ++i)
    result[i] = (qchar)$az_LineEditor_buffer->v[i];
  result[$az_LineEditor_buffer->len] = 0;

  $q_Workspace_echo_area_contents = result;

  if ($u_echo_off != ($v_LineEditor_echo_mode ?: $v_Workspace_echo_mode))
    $i_Workspace_echo_area_cursor = $i_LineEditor_cursor;
  else
    $i_Workspace_echo_area_cursor = -1;
}

/*
  SYMBOL: $f_LineEditor_get_text
    Sets $w_LineEditor_text to the current contents of the LineEditor.
 */
defun($h_LineEditor_get_text) {
  mwstring dst =
    gcalloc(sizeof(wchar_t)*(1 + $az_LineEditor_buffer->len));
  memcpy(dst, $az_LineEditor_buffer->v,
         $az_LineEditor_buffer->len * sizeof(wchar_t));
  $w_LineEditor_text = dst;
}

/*
  SYMBOL: $f_LineEditor_delete_backward_char
    Delete the character immediately before the cursor.
 */
defun($h_LineEditor_delete_backward_char) {
  if ($i_LineEditor_cursor != 0) {
    let($y_LineEditor_edit_is_minor, true);
    $f_LineEditor_push_undo();
    --$i_LineEditor_cursor;
    dynar_erase_z($az_LineEditor_buffer, $i_LineEditor_cursor, 1);

    $m_changed();
  }
}

/*
  SYMBOL: $f_LineEditor_delete_forward_char
    Delete the character immediately after the cursor.
 */
defun($h_LineEditor_delete_forward_char) {
  if ($i_LineEditor_cursor != $az_LineEditor_buffer->len) {
    let($y_LineEditor_edit_is_minor, true);
    $f_LineEditor_push_undo();
    dynar_erase_z($az_LineEditor_buffer, $i_LineEditor_cursor, 1);

    $m_changed();
  }
}

/*
  SYMBOL: $f_LineEditor_move_forward_char
    Moves the cursor one character to the right.

  SYMBOL: $I_LastCommand_forward_char
    The distance to move on the next LineEditor_move_forward_char. Zero means 1
    (as does 1).
 */
defun($h_LineEditor_move_forward_char) {
  unsigned dist = accelerate_max(
    &$I_LastCommand_forward_char,
    $az_LineEditor_buffer->len - $i_LineEditor_cursor);

  $i_LineEditor_cursor += dist;
  $m_changed();
}

/*
  SYMBOL: $f_LineEditor_move_backward_char
    Moves the cursor one character to the left.

  SYMBOL: $I_LastCommand_backward_char
    accelerate() value for $f_LineEditor_move_backward_char.
 */
defun($h_LineEditor_move_backward_char) {
  unsigned dist = accelerate_max(
    &$I_LastCommand_backward_char,
    $i_LineEditor_cursor);

  $i_LineEditor_cursor -= dist;
  $m_changed();
}

/*
  SYMBOL: $f_LineEditor_move_forward_word
    Moves the cursor forward one word, as defined by is_word_boundary().
 */
defun($h_LineEditor_move_forward_word) {
  if ($i_LineEditor_cursor == $az_LineEditor_buffer->len)
    // Already at end
    return;

  do {
    ++$i_LineEditor_cursor;
  } while ($i_LineEditor_cursor != $az_LineEditor_buffer->len &&
           !is_word_boundary(
             $az_LineEditor_buffer->v[$i_LineEditor_cursor-1],
             $az_LineEditor_buffer->v[$i_LineEditor_cursor  ]));

  $m_changed();
}

/*
  SYMBOL: $f_LineEditor_move_backward_word
    Moves the cursor backward one word, as defined by is_word_boundary().
 */
defun($h_LineEditor_move_backward_word) {
  if ($i_LineEditor_cursor == 0)
    // Already at the beginning
    return;

  do {
    --$i_LineEditor_cursor;
  } while ($i_LineEditor_cursor &&
           !is_word_boundary(
             $az_LineEditor_buffer->v[$i_LineEditor_cursor-1],
             $az_LineEditor_buffer->v[$i_LineEditor_cursor  ]));

  $m_changed();
}

/*
  SYMBOL: $f_LineEditor_kill
    Kills text between $i_LineEditor_kill and $i_LineEditor_cursor, including
    the lower bound and excluding the upper bound, placing the text on the kill
    ring. The cursor is moved to the lower bound.
 */
defun($h_LineEditor_kill) {
  if ($i_LineEditor_cursor == $i_LineEditor_kill) return;
  if ($i_LineEditor_cursor > $i_LineEditor_kill) {
    int tmp = $i_LineEditor_kill;
    $i_LineEditor_kill = $i_LineEditor_cursor;
    $i_LineEditor_cursor = tmp;

    // Cursor was ahead of the killed region, so killing backwards
    $v_kill_direction = $u_backward;
  } else {
    //Cursor was behind the killed region, so killing forwards
    $v_kill_direction = $u_forward;
  }

  int begin = $i_LineEditor_cursor;
  int end = $i_LineEditor_kill;
  mwstring text = gcalloc(sizeof(wchar_t)*(end-begin+1));
  memcpy(text,
         $az_LineEditor_buffer->v + begin,
         (end-begin)*sizeof(wchar_t));

  $F_c_kill(0,0, $w_kill = text);

  $m_push_undo();
  dynar_erase_z($az_LineEditor_buffer, begin, end-begin);
  $m_changed();
}

/*
  SYMBOL: $f_LineEditor_move_and_kill_between
    Saves $i_LineEditor_cursor into $i_LineEditor_kill, then calls
    $p_LineEditor_move_and_kill_between, which must be of type
      void (*)(void)
    afterward, it calls $m_kill().

  SYMBOL: $p_LineEditor_move_and_kill_between
    A pointer of type void (*)(void) which performs some kind of cursor
    movement within this LineEditor.
 */
defun($h_LineEditor_move_and_kill_between) {
  $i_LineEditor_kill = $i_LineEditor_cursor;
  ((void (*)(void))$p_LineEditor_move_and_kill_between)();
  $m_kill();
}

/*
  SYMBOL: $f_LineEditor_kill_forward_word
    Deletes characters between the cursor and the next word boundary, and adds
    the killed text to the character-oriented kill ring.
 */
defun($h_LineEditor_kill_forward_word) {
  $p_LineEditor_move_and_kill_between = $m_move_forward_word;
  $m_move_and_kill_between();
}

/*
  SYMBOL: $f_LineEditor_kill_backward_word
    Deletes characters between the cursor and the previous word boundary, and
    adds the killed text to the character-oriented kill ring.
 */
defun($h_LineEditor_kill_backward_word) {
  $p_LineEditor_move_and_kill_between = $m_move_backward_word;
  $m_move_and_kill_between();
}

/*
  SYMBOL: $f_LineEditor_home
    Moves cursor to the first non-whitespace character, or to column zero if it
    was already there.
 */
defun($h_LineEditor_home) {
  unsigned firstNonWhitespace = 0;
  while (firstNonWhitespace < $az_LineEditor_buffer->len &&
         iswspace($az_LineEditor_buffer->v[firstNonWhitespace]))
    ++firstNonWhitespace;

  if ($i_LineEditor_cursor == firstNonWhitespace)
    $i_LineEditor_cursor = 0;
  else
    $i_LineEditor_cursor = firstNonWhitespace;

  $m_changed();
}

/*
  SYMBOL: $f_LineEditor_end
    Moves the cursor past the last character in the line.
 */
defun($h_LineEditor_end) {
  $i_LineEditor_cursor = $az_LineEditor_buffer->len;
  $m_changed();
}

/*
  SYMBOL: $f_LineEditor_kill_to_bol
    Kills all text between the cursor and the beginning of the line.
 */
defun($h_LineEditor_kill_to_bol) {
  $p_LineEditor_move_and_kill_between = $m_home;
  $m_move_and_kill_between();
}

/*
  SYMBOL: $f_LineEditor_kill_to_eol
    Kills all text between the cursor and the end of the line.
 */
defun($h_LineEditor_kill_to_eol) {
  $p_LineEditor_move_and_kill_between = $m_end;
  $m_move_and_kill_between();
}

/*
  SYMBOL: $h_LineEditor_seek_forward_to_char $h_LineEditor_seek_forward_to_char_i
    Moves point forward by characters until the end of the buffer or
    $z_LineEditor_seek_dst is encountered.

  SYMBOL: $z_LineEditor_seek_dst
    Character to which to seek on target-based functions.
 */
interactive($h_LineEditor_seek_forward_to_char_i,
            $h_LineEditor_seek_forward_to_char,
            i_(z, $z_LineEditor_seek_dst, L"Seek")) {
  if ($i_LineEditor_cursor >= $az_LineEditor_buffer->len)
    return; //already at end

  ++$i_LineEditor_cursor;

  while ($i_LineEditor_cursor < $az_LineEditor_buffer->len &&
         $z_LineEditor_seek_dst !=
           $az_LineEditor_buffer->v[$i_LineEditor_cursor])
    ++$i_LineEditor_cursor;
  $m_changed();
}

/*
  SYMBOL: $f_LineEditor_seek_and_kill_forward_to_char
  SYMBOL: $f_LineEditor_seek_and_kill_forward_to_char_i
    Kills text between the cursor and the targetted character in
    $z_LineEditor_seek_dst.
 */
interactive($h_LineEditor_seek_and_kill_forward_to_char_i,
            $h_LineEditor_seek_and_kill_forward_to_char,
            i_(z, $z_LineEditor_seek_dst, L"Seek-Kill")) {
  $p_LineEditor_move_and_kill_between = $m_seek_forward_to_char;
  $m_move_and_kill_between();
}

/*
  SYMBOL: $h_LineEditor_seek_backward_to_char $h_LineEditor_seek_backward_to_char_i
    Moves point backward by characters until the beginning of the buffer or
    $z_LineEditor_seek_dst is encountered.
 */
interactive($h_LineEditor_seek_backward_to_char_i,
            $h_LineEditor_seek_backward_to_char,
            i_(z, $z_LineEditor_seek_dst, L"Seek")) {
  if (!$i_LineEditor_cursor) return; //Already at beginning

  --$i_LineEditor_cursor;

  while ($i_LineEditor_cursor > 0 &&
         $z_LineEditor_seek_dst !=
           $az_LineEditor_buffer->v[$i_LineEditor_cursor])
    --$i_LineEditor_cursor;
  $m_changed();
}

/*
  SYMBOL: $f_LineEditor_seek_and_kill_backward_to_char
  SYMBOL: $f_LineEditor_seek_and_kill_backward_to_char_i
    Kills text between the cursor and the targetted character in
    $z_LineEditor_seek_dst.
 */
interactive($h_LineEditor_seek_and_kill_backward_to_char_i,
            $h_LineEditor_seek_and_kill_backward_to_char,
            i_(z, $z_LineEditor_seek_dst, L"Seek-Kill")) {
  $p_LineEditor_move_and_kill_between = $m_seek_backward_to_char;
  $m_move_and_kill_between();
}

/*
  SYMBOL: $h_LineEditor_seek_forward_to_word_i $h_LineEditor_seek_forward_to_word
    Moves point forward by words until the end of the buffer or
    $z_LineEditor_seek_dst is encountered.
 */
interactive($h_LineEditor_seek_forward_to_word_i,
            $h_LineEditor_seek_forward_to_word,
            i_(z, $z_LineEditor_seek_dst, L"Seek")) {
  do {
    $f_LineEditor_move_forward_word();
  }  while ($i_LineEditor_cursor != $az_LineEditor_buffer->len &&
            $z_LineEditor_seek_dst !=
              $az_LineEditor_buffer->v[$i_LineEditor_cursor]);
}

/*
  SYMBOL: $f_LineEditor_seek_and_kill_forward_to_word
  SYMBOL: $f_LineEditor_seek_and_kill_forward_to_word_i
    Kills text between the cursor and the next word beginning with
    $z_LineEditor_seek_dst.
 */
interactive($h_LineEditor_seek_and_kill_forward_to_word_i,
            $h_LineEditor_seek_and_kill_forward_to_word,
            i_(z, $z_LineEditor_seek_dst, L"Seek-Kill")) {
  $p_LineEditor_move_and_kill_between = $m_seek_forward_to_word;
  $m_move_and_kill_between();
}

/*
  SYMBOL: $h_LineEditor_seek_backward_to_word_i $h_LineEditor_seek_backward_to_word
    Moves point backward by words until the end of the buffer or
    $z_LineEditor_seek_dst is encountered.
 */
interactive($h_LineEditor_seek_backward_to_word_i,
            $h_LineEditor_seek_backward_to_word,
            i_(z, $z_LineEditor_seek_dst, L"Seek")) {
  do {
    $f_LineEditor_move_backward_word();
  } while ($i_LineEditor_cursor &&
           $z_LineEditor_seek_dst !=
             $az_LineEditor_buffer->v[$i_LineEditor_cursor]);
}

/*
  SYMBOL: $f_LineEditor_seek_and_kill_backward_to_word_i
  SYMBOL: $f_LineEditor_seek_and_kill_backward_to_word
    Kills text between the cursor and the previous word beginning with
    $z_LineEditor_seek_dst.
 */
interactive($h_LineEditor_seek_and_kill_backward_to_word_i,
            $h_LineEditor_seek_and_kill_backward_to_word,
            i_(z, $z_LineEditor_seek_dst, L"Seek-Kill")) {
  $p_LineEditor_move_and_kill_between = $m_seek_backward_to_word;
  $m_move_and_kill_between();
}

/*
  SYMBOL: $f_LineEditor_yank_and_adv
    Inserts the text at the front of the character-oriented kill ring, then
    advances the cursor to be one past the end of the inserted string.
 */
defun($h_LineEditor_yank_and_adv) {
  if (!$aw_c_kill_ring->v[$I_c_kill_ring])
    return;

  $m_yank();
  $i_LineEditor_cursor += wcslen($aw_c_kill_ring->v[$I_c_kill_ring]);
  $m_changed();
}

/*
  SYMBOL: $f_LineEditor_yank_no_adv
     Inserts the text at the front of the character-oriented kill ring, and
     leaves the cursor where it was.
 */
defun($h_LineEditor_yank) {
  if (!$aw_c_kill_ring->v[$I_c_kill_ring])
    return;

  wstring to_insert = $aw_c_kill_ring->v[$I_c_kill_ring];
  $m_push_undo();
  dynar_ins_z($az_LineEditor_buffer, $i_LineEditor_cursor,
              to_insert, wcslen(to_insert));

  $m_changed();
}

/*
  SYMBOL: $lp_LineEditor_keybindings
    List of keybindings supported by generic LineEditors.
 */
class_keymap($c_LineEditor, $lp_LineEditor_keybindings, $llp_Activity_keymap)
ATSTART(setup_line_editor_keybindings, STATIC_INITIALISATION_PRIORITY) {
  bind_kp($lp_LineEditor_keybindings, $u_ground, KEYBINDING_DEFAULT, NULL,
          $m_self_insert);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'j', $v_end_meta,
            $m_move_backward_char);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'k', $v_end_meta,
            $m_move_forward_char);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'u', $v_end_meta,
            $m_move_backward_word);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'i', $v_end_meta,
            $m_move_forward_word);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'J', $v_end_meta,
            $m_seek_backward_to_char_i);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'K', $v_end_meta,
            $m_seek_forward_to_char_i);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'U', $v_end_meta,
            $m_seek_backward_to_word_i);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'I', $v_end_meta,
            $m_seek_forward_to_word_i);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'l', $v_end_meta,
            $m_delete_backward_char);
  bind_char($lp_LineEditor_keybindings, $u_meta, L';', $v_end_meta,
            $m_delete_forward_char);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'L', $v_end_meta,
            $m_seek_and_kill_backward_to_char_i);
  bind_char($lp_LineEditor_keybindings, $u_meta, L':', $v_end_meta,
            $m_seek_and_kill_forward_to_char_i);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'o', $v_end_meta,
            $m_kill_backward_word);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'p', $v_end_meta,
            $m_kill_forward_word);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'O', $v_end_meta,
            $m_seek_and_kill_backward_to_word_i);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'P', $v_end_meta,
            $m_seek_and_kill_forward_to_word_i);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'h', $v_end_meta,
            $m_home);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'H', $v_end_meta,
            $m_kill_to_bol);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'n', $v_end_meta,
            $m_end);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'N', $v_end_meta,
            $m_kill_to_eol);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'b', $v_end_meta,
            $m_yank_and_adv);
  bind_char($lp_LineEditor_keybindings, $u_meta, L'B', $v_end_meta,
            $m_yank);
  bind_char($lp_LineEditor_keybindings, $u_ground, L'\r', NULL,
            $m_accept);
}
