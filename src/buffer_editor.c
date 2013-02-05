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
    Identifies by class the BufferEditor's hook on $H_shunt within a
    FileBufferCursor to refresh the echo area.

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

  $$($o_BufferEditor_buffer) {
    lpush_o($lo_FileBuffer_attachments, $o_BufferEditor);
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

  $$($o_BufferEditor_buffer) {
    $lo_FileBuffer_attachments = lrm_o($lo_FileBuffer_attachments,
                                       $o_BufferEditor);
  }

  $f_Activity_destroy();
}

/*
  SYMBOL: $h_BufferEditor_get_face
    Updates $I_BufferEditor_face.

  SYMBOL: $I_BufferEditor_face
    The current face to apply to the buffer's name.
 */
defun($h_BufferEditor_get_face) {
  $$($o_BufferEditor_buffer) {
    if ($y_FileBuffer_modified)
      $I_BufferEditor_face = $I_BufferEditor_modified_face;
    else if ($y_FileBuffer_readonly)
      $I_BufferEditor_face = $I_BufferEditor_readonly_face;
    else
      $I_BufferEditor_face = $I_BufferEditor_unmodified_face;
  }
}

/*
  SYMBOL: $f_BufferEditor_get_echo_area_contents
    Sets $q_Workspace_echo_area_contents to the string under the cursor.
 */
defun($h_BufferEditor_get_echo_area_contents) {
  $$($o_BufferEditor_buffer) $$($o_BufferEditor_cursor) {
    $m_access();

    if ($I_FileBufferCursor_line_number <
        $aw_FileBuffer_contents->len) {
      $q_Workspace_echo_area_contents =
        $M_cvt($q_RenderedLine_cvt,
               $M_format($lo_BufferEditor_format, 0,
                         $I_BufferEditor_index =
                           $I_FileBufferCursor_line_number)->car);
    } else {
      $q_Workspace_echo_area_contents = qempty;
    }
  }
}

/*
  SYMBOL: $f_BufferEditor_is_echo_enabled
    Sets $y_Workspace_is_echo_enabled according to $v_Workspace_echo_mode.
 */
defun($h_BufferEditor_is_echo_enabled) {
  $y_Workspace_is_echo_enabled = ($v_Workspace_echo_mode == $u_echo_on);
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
  static qchar lparen[2] = { L'(', 0 }, rparen[2] = { L')', 0 };

  qstring inner = qempty;

  object next = NULL;
  if ($lo_echo_area_activities) {
    next = $lo_echo_area_activities->car;
    if (next != $o_BufferLineEditor_parent) {
      let($lo_echo_area_activities, $lo_echo_area_activities->cdr);
      $M_get_echo_area_meta(0, next);
    } else {
      list_o remaining = $lo_echo_area_activities->cdr;
      let($lo_echo_area_activities, NULL);
      inner = $M_get_echo_area_meta($q_Workspace_echo_area_meta, next);

      $q_Workspace_echo_area_meta = qempty;
      if (remaining) {
        let($lo_echo_area_activities, remaining->cdr);
        $M_get_echo_area_meta(0, remaining->car);
      }
    }
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
    qstring parts[] = { lparen, inner,
                        wstrtoqstr(linenum), rparen, qspace,
                        $q_Workspace_echo_area_meta };
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
  unsigned line_number = $($o_BufferLineEditor_cursor, $I_FileBufferCursor_line_number);

  // We no longer care about window notifications (and we're about to trigger
  // one anyway)
  $$($o_BufferLineEditor_cursor) {
    del_hook($H_window_changed, HOOK_MAIN,
             $u_BufferLineEditor, $o_BufferLineEditor);
  }
  $M_edit(0, $o_BufferLineEditor_buffer,
          $I_FileBuffer_ndeletions = ($y_BufferLineEditor_replace? 1:0),
          $lw_FileBuffer_replacements = cons_w(line, NULL),
          $I_FileBuffer_edit_line = line_number);
  // If echo is on, output the new line to the Transcript
  if (($v_LineEditor_echo_mode ?: $v_Workspace_echo_mode) == $u_echo_on) {
    $M_echo_line(0, $o_BufferLineEditor_parent,
                 $I_BufferEditor_index = line_number);
  }
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
  SYMBOL: $f_BufferEditor_insert_and_edit
    Insert a new line before the cursor with a BufferLineEditor. When the line
    editor is accepted, cursor will be shunted downward, and this method will
    be called again.

  SYMBOL: $u_continue_inserting
    Identifies hooks used to continue in "insert mode" after a line editor is
    accepted.
 */
defun($h_BufferEditor_insert_and_edit) {
  object editor = $c_BufferLineEditor(
    $i_LineEditor_cursor = -1,
    $w_LineEditor_text = NULL);
  $$(editor) {
    add_hook_obj($H_accept, HOOK_AFTER,
                 $u_continue_inserting, $u_BufferEditor,
                 $m_insert_and_edit, $o_BufferEditor,
                 NULL);
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
    $m_access();
    unsigned dist = accelerate_max(&$I_LastCommand_forward_line,
                                   $aw_FileBuffer_contents->len -
                                   $I_FileBufferCursor_line_number);
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
  SYMBOL: $f_BufferEditor_kill_forward_line
    Kills the line in front of the cursor, saving it to the kill ring.
 */
defun($h_BufferEditor_kill_forward_line) {
  $$($o_BufferEditor_buffer) {
    $m_access();
    $$($o_BufferEditor_cursor) {
      if ($I_FileBufferCursor_line_number != $aw_FileBuffer_contents->len) {
        $F_l_kill(0,0,
                  $lw_kill = cons_w(
                    $aw_FileBuffer_contents->v[$I_FileBufferCursor_line_number],
                    NULL),
                  $v_kill_direction = $u_forward);
        $M_edit(0,0,
                $I_FileBuffer_ndeletions = 1,
                $lw_FileBuffer_replacements = NULL,
                $I_FileBuffer_edit_line = $I_FileBufferCursor_line_number);
      }
    }
  }
}

/*
  SYMBOL: $f_BufferEditor_kill_backward_line
    Kills the line behind the cursor, saving it to the kill ring.
 */
defun($h_BufferEditor_kill_backward_line) {
  $$($o_BufferEditor_buffer) {
    $m_access();
    $$($o_BufferEditor_cursor) {
      if ($I_FileBufferCursor_line_number) {
        $F_l_kill(0,0,
                  $lw_kill = cons_w(
                    $aw_FileBuffer_contents->v[$I_FileBufferCursor_line_number-1],
                    NULL),
                  $v_kill_direction = $u_backward);
        $M_edit(0,0,
                $I_FileBuffer_ndeletions = 1,
                $lw_FileBuffer_replacements = NULL,
                $I_FileBuffer_edit_line = $I_FileBufferCursor_line_number-1);
      }
    }
  }
}

/*
  SYMBOL: $f_BufferEditor_home
    Moves the cursor back to the first line.
 */
defun($h_BufferEditor_home) {
  $$($o_BufferEditor_cursor) {
    let($i_FileBufferCursor_shunt_distance,
        -(signed)$I_FileBufferCursor_line_number);
    $m_shunt();
  }
}

/*
  SYMBOL: $f_BufferEditor_end
    Moves the cursor forward to one line after the last line.
 */
defun($h_BufferEditor_end) {
  $$($o_BufferEditor_buffer) {
    $m_access();
    $$($o_BufferEditor_cursor) {
      let($i_FileBufferCursor_shunt_distance,
          $aw_FileBuffer_contents->len -
            $I_FileBufferCursor_line_number);
      $m_shunt();
    }
  }
}

/*
  SYMBOL: $f_BufferEditor_show_forward_line
    Show the line(s) below the cursor in the current Transcript.

  SYMBOL: $I_LastCommand_show_forward_line
    Param to accelerate() for $f_BufferEditor_show_forward_line.

  SYMBOL: $I_LastCommand_show_forward_line_off
    The offset from previous calls to $f_BufferEditor_show_forward_line.
 */
defun($h_BufferEditor_show_forward_line) {
  $$($o_BufferEditor_buffer) {
    $m_access();
    $$($o_BufferEditor_cursor) {
      unsigned offset = $($o_prev_command, $I_LastCommand_show_forward_line_off);
      unsigned cnt = accelerate_max(&$I_LastCommand_show_forward_line,
                                    $aw_FileBuffer_contents->len -
                                      $I_FileBufferCursor_line_number -
                                      offset);

      $$($o_this_command) {
        $I_LastCommand_show_forward_line_off = offset+cnt;
      }

      $lo_BufferEditor_format = NULL;
      for (unsigned i = 0; i < cnt; ++i) {
        $I_BufferEditor_index =
          $I_FileBufferCursor_line_number + offset + cnt - i - 1;
        $m_format();
      }

      if ($o_Transcript)
        $M_group(0, $o_Transcript,
                 $lo_Transcript_output = $lo_BufferEditor_format);
      $lo_BufferEditor_format = NULL;
    }
  }
}

/*
  SYMBOL: $f_BufferEditor_show_backward_line
    Shows the line(s) before the cursor in the current Transcript.

  SYMBOL: $I_LastCommand_show_backward_line_off
    The offset from the cursor of the last call to
    $f_BufferEditor_show_backward_line.

  SYMBOL: $I_LastCommand_show_backward_line
    Parameter to accelerate() for $f_BufferEditor_show_backward_line.
 */
defun($h_BufferEditor_show_backward_line) {
  $$($o_BufferEditor_buffer) {
    $m_access();
    $$($o_BufferEditor_cursor) {
      if (!$I_FileBufferCursor_line_number) return;

      unsigned offset = $($o_prev_command,
                          $I_LastCommand_show_backward_line_off);
      unsigned cnt = accelerate_max(&$I_LastCommand_show_backward_line,
                                    $I_FileBufferCursor_line_number -
                                    offset);

      $$($o_this_command) {
        $I_LastCommand_show_backward_line_off = offset+cnt;
      }

      $lo_BufferEditor_format = NULL;
      for (unsigned i = 0; i < cnt; ++i) {
        $I_BufferEditor_index =
          $I_FileBufferCursor_line_number - offset - cnt + i;
        $m_format();
      }

      if ($o_Transcript) {
        $M_group(0, $o_Transcript,
                 $lo_Transcript_output = $lo_BufferEditor_format);
        $lo_BufferEditor_format = NULL;
      }
    }
  }
}

/*
  SYMBOL: $f_BufferEditor_echo_line
    Echos a single line to the Transcript (if any), as an append (versus an
    output group). The line to output is indicated by $I_BufferEditor_index.
 */
defun($h_BufferEditor_echo_line) {
  $$($o_BufferEditor_buffer) {
    $m_access();

    $lo_BufferEditor_format = NULL;
    $m_format();

    if ($o_Transcript)
      $M_append(0, $o_Transcript,
                $lo_Transcript_output = $lo_BufferEditor_format);
    $lo_BufferEditor_format = NULL;
  }
}

/*
  SYMBOL: $f_BufferEditor_format $lo_BufferEditor_format $I_BufferEditor_index
    Converts the line at $I_BufferEditor_index into one or more RenderedLines
    prepended to $o_BufferEditor_format. This will be called within the context
    of the FileBuffer.

  SYMBOL: $I_BufferEditor_line_wrap_meta_face
    Face to apply to metadata for wrapped fragments of lines.
 */
defun($h_BufferEditor_format) {
  // Get the base RenderedLine
  object base = $c_RenderedLine(
    $q_RenderedLine_body = wstrtoqstr($aw_FileBuffer_contents->v[
                                        $I_BufferEditor_index]),
    $q_RenderedLine_meta = NULL);

  // Apply syntax highlighting, etc
  $$(base) {
    $m_prettify();
  }

  // Split into multiple lines
  $M_line_wrap_reverse(0,0,
                       $lq_BufferEditor_wrapped_rev = NULL,
                       $q_BufferEditor_line_wrap_reverse =
                         $(base, $q_RenderedLine_body));

  qchar awrapped[$i_line_meta_width + 1];
  for (int i = 0; i < $i_line_meta_width; ++i)
    awrapped[i] = apply_face($I_BufferEditor_line_wrap_meta_face,
                             L'/');
  awrapped[$i_line_meta_width] = 0;
  qstring wrapped = qstrdup(awrapped);

  // Combine into list
  for (list_q curr = $lq_BufferEditor_wrapped_rev;
       curr; curr = curr->cdr) {
    lpush_o($lo_BufferEditor_format,
            $c_RenderedLine(
              $q_RenderedLine_body = curr->car,
              // For the first line (the last in this list), use the original
              // metadata; for the others, just indicate it is a line
              // continuation.
              $q_RenderedLine_meta =
                curr->cdr?
                wrapped :
                $(base, $q_RenderedLine_meta)));
  }
}

/*
  SYMBOL: $f_BufferEditor_line_wrap_reverse
    Breaks the input string ($q_BufferEditor_line_wrap_reverse) into lines
    which fit within $i_column_width, and prepending the fragments to
    $lq_BufferEditor_wrapped_rev, such that the first fragment is the last in
    the list. The basic implementation just hard splits the input line every
    $i_column_width characters.

  SYMBOL: $lq_BufferEditor_wrapped_rev
    Output from $f_BufferEditor_line_wrap_reverse.
 */
defun($h_BufferEditor_line_wrap_reverse) {
  while (qstrlen($q_BufferEditor_line_wrap_reverse) >
         (unsigned)$i_column_width) {
    mqstring new = gcalloc(sizeof(qchar) * (1+$i_column_width));
    memcpy(new, $q_BufferEditor_line_wrap_reverse,
           sizeof(qchar)*(1 + $i_column_width));
    lpush_q($lq_BufferEditor_wrapped_rev, new);
    $q_BufferEditor_line_wrap_reverse += $i_column_width;
  }

  lpush_q($lq_BufferEditor_wrapped_rev, $q_BufferEditor_line_wrap_reverse);
}

/*
  SYMBOL: $f_BufferEditor_save
    Saves the contents of the buffer to the current filename.
 */
defun($h_BufferEditor_save) {
  $M_save(0, $o_BufferEditor_buffer);
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
  bind_char($lp_BufferEditor_keymap, $u_ground, L'o', NULL,
            $f_BufferEditor_insert_blank_line_below);
  bind_char($lp_BufferEditor_keymap, $u_ground, L'e', NULL,
            $f_BufferEditor_edit_current);
  bind_char($lp_BufferEditor_keymap, $u_ground, L'i', NULL,
            $f_BufferEditor_insert_and_edit);

  bind_char($lp_BufferEditor_keymap, $u_extended, CONTROL_S, $u_ground,
            $f_BufferEditor_save);

  bind_char($lp_BufferEditor_keymap, $u_meta, L'j', $v_end_meta,
            $f_BufferEditor_backward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'k', $v_end_meta,
            $f_BufferEditor_forward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'l', $v_end_meta,
            $f_BufferEditor_kill_backward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L';', $v_end_meta,
            $f_BufferEditor_kill_forward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'h', $v_end_meta,
            $f_BufferEditor_home);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'n', $v_end_meta,
            $f_BufferEditor_end);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'f', $v_end_meta,
            $f_BufferEditor_show_forward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'd', $v_end_meta,
            $f_BufferEditor_show_backward_line);
}
