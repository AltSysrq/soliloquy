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

  qstring parts[6] = { lparen, name,
                       wstrtoqstr(linenum),
                       apply_face_str($I_BufferEditor_mark_delta_face,
                                      wstrtoqstr(markline)),
                       rparen,
                       $q_Workspace_echo_area_meta };
  $q_Workspace_echo_area_meta = qstrapv(parts, 6);
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
  $o_BufferLineEditor_cursor = object_clone($o_BufferEditor_cursor);
  $o_BufferLineEditor_parent = $o_BufferEditor;
  $o_BufferLineEditor_buffer = $o_BufferEditor_buffer;
}

/*
  SYMBOL: $o_BufferLineEditor_destroy
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
    rparen = wstrtoqstr(L"]");
  }
  mqstring name = wstrtoqstr($($o_BufferLineEditor_buffer,
                               $w_FileBuffer_filename));
  apply_face_str($M_get_face($I_BufferEditor_face,
                             $o_BufferLineEditor_parent),
                 name);
  wchar_t linenum[16];
  swprintf(linenum, 16, L":%d", 1+$($o_BufferLineEditor_cursor,
                                    $I_FileBufferCursor_line_number));

  qstring parts[5] = { lparen, name, wstrtoqstr(linenum), rparen,
                       $q_Workspace_echo_area_meta };
  $q_Workspace_echo_area_meta = qstrapv(parts, 5);
}

/*
  SYMBOL: $f_BufferLineEditor_accept
    Accepts the new text of the line, and performs the edit within the buffer.
 */
defun($h_BufferLineEditor_accept) {
  mwstring line = gcalloc(sizeof(wchar_t)*($az_LineEditor_buffer->len + 1));
  memcpy(line, $az_LineEditor_buffer->v,
         sizeof(wchar_t)*$az_LineEditor_buffer->len);
  $M_edit(0, $o_BufferLineEditor_buffer,
          $I_FileBuffer_ndeletions = ($y_BufferLineEditor_replace? 1:0),
          $lw_FileBuffer_replacements = cons_w(line, NULL),
          $I_FileBuffer_edit_line =
            $($o_BufferLineEditor_cursor, $I_FileBufferCursor_line_number));
  $m_destroy();
}