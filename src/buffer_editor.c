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
#include "buffer_editor.slc"

#include "face.h"
#include "interactive.h"
#include "key_dispatch.h"

/*
  TITLE: Buffer Editor
  OVERVIEW: Provides an Activity for editing FileBuffers.
*/


/*
  SYMBOL: $I_BufferEditor_unmodified_face
    Face to apply to the buffer name when the buffer is unmodified.

  SYMBOL: $I_BufferEditor_modified_face
    Face to apply to the buffer name when the buffer has been modified.

  SYMBOL: $I_BufferEditor_readonly_face
    Face to apply to the buffer name when the buffer is marked readonly.

  SYMBOL: $I_BufferEditor_mark_delta_face
    The face to apply to the mark delta indicator in the meta area.
 */
ATSINIT {
  $I_BufferEditor_unmodified_face = mkface("*fw");
  $I_BufferEditor_modified_face = mkface("+B*fY");
  $I_BufferEditor_readonly_face = mkface("+I+U");
  $I_BufferEditor_mark_delta_face = mkface("*fc");
}

/*
  SYMBOL: $c_BufferEditor
    An Activity which allows editing a FileBuffer.

  SYMBOL: $o_BufferEditor_buffer
    The FileBuffer which this BufferEditor is currently editing.

  SYMBOL: $o_BufferEditor_cursor
    The cursor into the current buffer. If this is NULL when the BufferEditor
    is constructed, it is set to point to the first line in
    BufferEditor_buffer.

  SYMBOL: $u_shunt_notify
    Identifies the BufferEditor's hook on $H_shunt within a FileBufferCursor to
    refresh the echo area.

  SYMBOL: $lo_BufferEditor_marks
    The mark stack (of FileBufferCursors) for this BufferEditor.
 */
subclass($c_Activity, $c_BufferEditor)
defun($h_BufferEditor) {
  if (!$o_BufferEditor_cursor) {
    $o_BufferEditor_cursor =
      $c_FileBufferCursor($o_FileBufferCursor_buffer =
                          $o_BufferEditor_buffer);
  }

  $$($o_BufferEditor_cursor) {
    add_hook_obj($H_shunt, HOOK_AFTER,
                 $u_BufferEditor, $u_shunt_notify,
                 $f_Workspace_update_echo_area, $o_Activity_workspace,
                 NULL);
  }
}

/*
  SYMBOL: $f_BufferEditor_destroy
    Destroys this BufferEditor, releasing all cursors it currently holds.
 */
defun($h_BufferEditor_destroy) {
  $M_destroy(0, $o_BufferEditor_cursor);
  each_o($lo_BufferEditor_marks,
         lambdav((object o), $M_destroy(0,o)));

  $f_Activity_destroy();
}

/*
  SYMBOL: $h_BufferEditor_get_face
    Updates $I_BufferEditor_face.

  SYMBOL: $I_BufferEditor_face
    The current face to apply to the buffer's name.
 */
defun($h_BufferEditor_get_face) {
  //TODO
  $I_BufferEditor_face = $I_BufferEditor_unmodified_face;
}

/*
  SYMBOL: $f_BufferEditor_get_echo_area_meta
    Adds the name of the buffer being edited and its line number (and mark
    delta, if applicable) to $q_Workspace_echo_area_meta.
 */
defun($h_BufferEditor_get_echo_area_meta) {
  if ($lo_echo_area_activities) {
    object next = $lo_echo_area_activities->car;
    let($lo_echo_area_activities, $lo_echo_area_activities->cdr);
    $M_get_echo_area_meta(0, next);
  }
  
  static qstring lparen = NULL, rparen = NULL;
  if (!lparen) {
    lparen = wstrtoqstr(L"[");
    rparen = wstrtoqstr(L"]");
  }

  mqstring name = wstrtoqstr($($o_BufferEditor_buffer, $w_FileBuffer_filename));
  apply_face_str($M_get_face($I_BufferEditor_face, 0), name);

  wchar_t linenum[16], markline[16];
  markline[0] = 0;
  swprintf(linenum, 16, L":%d", 1+$($o_BufferEditor_cursor,
                                    $I_FileBufferCursor_line_number));
  if ($lo_BufferEditor_marks)
    swprintf(markline, 16, L"%+d",
             ((signed)$($lo_BufferEditor_marks->car,
                        $I_FileBufferCursor_line_number)) -
             ((signed)$($o_BufferLineEditor_cursor,
                        $I_FileBufferCursor_line_number)));

  qstring parts[] = { lparen, name,
                      wstrtoqstr(linenum),
                      apply_face_str($I_BufferEditor_mark_delta_face,
                                     wstrtoqstr(markline)),
                      rparen,
                      $lo_echo_area_activities? qspace : qempty,
                      $q_Workspace_echo_area_meta };
  $q_Workspace_echo_area_meta = qstrapv(parts, lenof(parts));
}

/*
  SYMBOL: $c_BufferLineEditor
    A LineEditor subclass which operates within a BufferEditor. It performs
    single-line insertions or replacements.

  SYMBOL: $o_BufferLineEditor_cursor
    The cursor position at which the insertion or replacement will occur.

  SYMBOL: $o_BufferLineEditor_buffer
    The buffer which this BufferLineEditor will edit.

  SYMBOL: $o_BufferLineEditor_parent
    The BufferEditor which this BufferLineEditor works within.
 */
subclass($c_LineEditor, $c_BufferLineEditor)
defun($h_BufferLineEditor) {
  // Clone won't work here because it doesn't set the new "this" value and
  // such. For now, just do it by constructing a new object within the
  // context of the other (which has the effect of passing all parms).
  //$o_BufferLineEditor_cursor = object_clone($o_BufferEditor_cursor);
  $o_BufferLineEditor_cursor =
    within_context($o_BufferEditor_cursor, $c_FileBufferCursor());
  $o_BufferLineEditor_parent = $o_BufferEditor;
  $o_BufferLineEditor_buffer = $o_BufferEditor_buffer;

  //We inherit the echo area update hook by cloning the BufferEditor's
  //cursor. But we need to know if our line gets deleted/modified, in which
  //case we must cease to exist.
  $$($o_BufferLineEditor_cursor) {
    $I_FileBufferCursor_window = 1;
    add_hook_obj($H_window_changed, HOOK_MAIN,
                 $u_BufferLineEditor, $u_BufferLineEditor,
                 $m_destroy, $o_BufferLineEditor,
                 NULL);
  }

  $M_update_echo_area(0, $o_Activity_workspace);
}

advise_before_superconstructor($h_BufferLineEditor) {
  $o_Activity_parent = $o_BufferEditor;
}

/*
  SYMBOL: $f_BufferLineEditor_abort
    Destroys this BufferLineEditor.
 */
defun($h_BufferLineEditor_abort) {
  $m_destroy();
}

/*
  SYMBOL: $f_BufferLineEditor_destroy
    Releases the resources used by this BufferLineEditor.
 */
defun($h_BufferLineEditor_destroy) {
  $M_destroy(0, $o_BufferLineEditor_cursor);
  $f_LineEditor_destroy();
}

/*
  SYMBOL: $f_BufferLineEditor_get_echo_area_meta
    Adds the name of the buffer being edited and its line number to
    $q_Workspace_echo_area_meta.
 */
defun($h_BufferLineEditor_get_echo_area_meta) {
  static qstring lparen = NULL, rparen = NULL;
  if (!lparen) {
    lparen = wstrtoqstr(L"(");
    rparen = wstrtoqstr(L")");
  }

  object next = NULL;
  if ($lo_echo_area_activities) {
    next = $lo_echo_area_activities->car;
    let($lo_echo_area_activities, $lo_echo_area_activities->cdr);
    $M_get_echo_area_meta(0, next);
  }

  wchar_t linenum[16] = {0};
  // Only indicate line number if we are not directly on top of our parent, or
  // if the line numbers diverge
  if (next != $o_BufferLineEditor_parent ||
      $($o_BufferLineEditor_cursor, $I_FileBufferCursor_line_number) !=
      $($($o_BufferLineEditor_parent, $o_BufferEditor_cursor),
        $I_FileBufferCursor_line_number))
    swprintf(linenum, 16, L":%d", 1+$($o_BufferLineEditor_cursor,
                                      $I_FileBufferCursor_line_number));

  if (next != $o_BufferLineEditor_parent) {
    // Not directly on parent, must use explicit syntax
    mqstring name = wstrtoqstr($($o_BufferLineEditor_buffer,
                                 $w_FileBuffer_filename));
    apply_face_str($M_get_face($I_BufferEditor_face,
                               $o_BufferLineEditor_parent),
                   name);
    qstring parts[] = { lparen, name, wstrtoqstr(linenum), rparen,
                        qspace, $q_Workspace_echo_area_meta };
    $q_Workspace_echo_area_meta = qstrapv(parts, lenof(parts));
  } else {
    // On top of parent, use concise syntax
    qstring parts[] = { lparen, $q_Workspace_echo_area_meta,
                        wstrtoqstr(linenum), rparen };
    $q_Workspace_echo_area_meta = qstrapv(parts, lenof(parts));
  }
}

/*
  SYMBOL: $f_BufferLineEditor_accept
    Accepts the new text of the line, and performs the edit within the buffer.

  SYMBOL: $y_BufferLineEditor_replace
    Whether the BufferLineEditor will replace a line, or insert a new one.
 */
defun($h_BufferLineEditor_accept) {
  mwstring line = gcalloc(sizeof(wchar_t)*($az_LineEditor_buffer->len + 1));
  memcpy(line, $az_LineEditor_buffer->v,
         sizeof(wchar_t)*$az_LineEditor_buffer->len);
  // We no longer care about window notifications (and we're about to trigger
  // one anyway)
  $$($o_BufferLineEditor_cursor) {
    del_hook($H_window_changed, HOOK_MAIN,
             $u_BufferLineEditor, $o_BufferLineEditor);
  }
  $M_edit(0, $o_BufferLineEditor_buffer,
          $I_FileBuffer_ndeletions = ($y_BufferLineEditor_replace? 1:0),
          $lw_FileBuffer_replacements = cons_w(line, NULL),
          $I_FileBuffer_edit_line =
            $($o_BufferLineEditor_cursor, $I_FileBufferCursor_line_number));
  $m_destroy();
}

/*
  SYMBOL: $f_BufferEditor_insert_blank_line_above
    Inserts a blank line above the current line. The cursor will be shunted
    downward by one line as a result.
 */
defun($h_BufferEditor_insert_blank_line_above) {
  $M_edit(0, $o_BufferEditor_buffer,
          $I_FileBuffer_ndeletions = 0,
          $lw_FileBuffer_replacements = cons_w(L"", NULL),
          $I_FileBuffer_edit_line =
            $($o_BufferEditor_cursor, $I_FileBufferCursor_line_number));
}

/*
  SYMBOL: $f_BufferEditor_insert_blank_line_below
    Inserts a blank line below the cursor, without advancing. If cursor is at
    the end of the buffer, a line is inserted before the cursor, then the
    cursor is retreated one line.
 */
defun($h_BufferEditor_insert_blank_line_below) {
  unsigned where = $($o_BufferEditor_cursor, $I_FileBufferCursor_line_number);
  $M_access(0, $o_BufferEditor_buffer);
  if (where < $($o_BufferEditor_buffer, $aw_FileBuffer_contents)->len) {
    $M_edit(0, $o_BufferEditor_buffer,
            $I_FileBuffer_ndeletions = 0,
            $lw_FileBuffer_replacements = cons_w(L"", NULL),
            $I_FileBuffer_edit_line = 1 + where);
  } else {
    $M_edit(0, $o_BufferEditor_buffer,
            $I_FileBuffer_ndeletions = 0,
            $lw_FileBuffer_replacements = cons_w(L"", NULL),
            $I_FileBuffer_edit_line = where);
    $M_shunt(0, $o_BufferEditor_cursor,
             $i_FileBufferCursor_shunt_distance = -1);
  }
}

/*
  SYMBOL: $f_BufferEditor_edit_current
    Opens a BufferLineEditor for the current line.
 */
defun($h_BufferEditor_edit_current) {
  $$($o_BufferEditor_buffer) {
    $m_access();
    unsigned where = $($o_BufferEditor_cursor,
                       $I_FileBufferCursor_line_number);
    wstring text = L"";
    if (where < $aw_FileBuffer_contents->len)
      text = $aw_FileBuffer_contents->v[where];
    // We need to temporarily NULL our keybindings so that the BufferLineEditor
    // doesn't inherit them.
    $c_BufferLineEditor($w_LineEditor_text = text,
                        $y_BufferLineEditor_replace =
                          (where < $aw_FileBuffer_contents->len));
  }
}

/*
  SYMBOL: $f_BufferEditor_self_insert
    Insert a new line before cursor, invoke an editor on it, call
    $m_self_insert in its context. As a side-effect, cursor will be shunted
    downward.
 */
defun($h_BufferEditor_self_insert) {
  if (!is_nc_char($x_Terminal_input_value)) {
    $y_key_dispatch_continue = true;
    return;
  }

  object editor = $c_BufferLineEditor();
  $M_self_insert(0, editor);
}

/*
  SYMBOL: $f_BufferEditor_forward_line
    Moves the buffer cursor down one line, unless already at the end of the
    file.

  SYMBOL: $I_LastCommand_forward_line
    Variable for accelerate() in $f_BufferEditor_forward_line.
 */
defun($h_BufferEditor_forward_line) {
  $$($o_BufferEditor_cursor) $$($o_BufferEditor_buffer) {
    unsigned dist = accelerate_max(&$I_LastCommand_forward_line,
                                   $aw_FileBuffer_contents->len -
                                   $I_FileBufferCursor_line_number);
    $m_access();
    $I_FileBufferCursor_line_number += dist;
  }

  $m_update_echo_area();
}

/*
  SYMBOL: $f_BufferEditor_backward_line
    Moves the buffer cursor up one line, unless already at the end of the
    file.

  SYMBOL: $I_LastCommand_backward_line
    Variable for accelerate() in $f_BufferEditor_backward_line.
 */
defun($h_BufferEditor_backward_line) {
  $$($o_BufferEditor_cursor) {
    unsigned dist = accelerate_max(&$I_LastCommand_backward_line,
                                   $I_FileBufferCursor_line_number);
    $I_FileBufferCursor_line_number -= dist;
  }

  $m_update_echo_area();
}

/*
  SYMBOL: $lp_BufferEditor_keymap
    Keybindings specific to BufferEditors.
 */
class_keymap($c_BufferEditor, $lp_BufferEditor_keymap, $llp_Activity_keymap)
ATSINIT {
  bind_char($lp_BufferEditor_keymap, $u_ground, L'\r', NULL,
            $f_BufferEditor_insert_blank_line_above);
  bind_char($lp_BufferEditor_keymap, $u_ground, CONTROL_O, NULL,
            $f_BufferEditor_insert_blank_line_below);
  bind_char($lp_BufferEditor_keymap, $u_ground, CONTROL_E, NULL,
            $f_BufferEditor_edit_current);

  bind_kp($lp_BufferEditor_keymap, $u_ground, KEYBINDING_DEFAULT, NULL,
          $f_BufferEditor_self_insert);

  bind_char($lp_BufferEditor_keymap, $u_meta, L'j', $v_end_meta,
            $f_BufferEditor_backward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'k', $v_end_meta,
            $f_BufferEditor_forward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'l', $v_end_meta,
            $f_BufferEditor_kill_backward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L';', $v_end_meta,
            $f_BufferEditor_kill_forward_line);
}
