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

  SYMBOL: $o_BufferEditor_point
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
  // Add hooks for cursor modification notification
  if ($o_BufferEditor_point) {
    $$($o_BufferEditor_point) {
      $m_attach_cursor();
    }
  }
  implant($h_FileBufferCursor);
  add_hook_obj(&$h_FileBufferCursor, HOOK_AFTER,
               $u_BufferEditor, $u_shunt_notify,
               $m_attach_cursor, $o_BufferEditor,
               NULL);

  if (!$o_BufferEditor_point) {
    $o_BufferEditor_point =
      $c_FileBufferCursor($o_FileBufferCursor_buffer =
                          $o_BufferEditor_buffer);
  }

  $$($o_BufferEditor_buffer) {
    lpush_o($lo_FileBuffer_attachments, $o_BufferEditor);
  }
}

/*
  SYMBOL: $f_BufferEditor_attach_cursor
    Must be called within the context of the BufferEditor and a
    FileBufferCursor. Adds a hook to update the echo area when the cursor is
    shunted.
 */
defun($h_BufferEditor_attach_cursor) {
  add_hook_obj($H_shunt, HOOK_AFTER,
               $u_BufferEditor, $u_shunt_notify,
               $f_Workspace_update_echo_area, $o_Activity_workspace,
               NULL);
}

/*
  SYMBOL: $f_BufferEditor_destroy
    Destroys this BufferEditor, releasing all cursors it currently holds.
 */
defun($h_BufferEditor_destroy) {
  $M_destroy(0, $o_BufferEditor_point);
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
    Sets $q_Workspace_echo_area_contents to the string under point.
 */
defun($h_BufferEditor_get_echo_area_contents) {
  $$($o_BufferEditor_buffer) $$($o_BufferEditor_point) {
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
  swprintf(linenum, 16, L":%d", 1+$($o_BufferEditor_point,
                                    $I_FileBufferCursor_line_number));
  if ($lo_BufferEditor_marks)
    swprintf(markline, 16, L"%+d",
             ((signed)$($lo_BufferEditor_marks->car,
                        $I_FileBufferCursor_line_number)) -
             ((signed)$($o_BufferEditor_point,
                        $I_FileBufferCursor_line_number)));

  qstring parts[] = { lparen, name,
                      wstrtoqstr(linenum),
                      apply_face_str($I_BufferEditor_mark_delta_face,
                                     wstrtoqstr(markline)),
                      rparen,
                      $lo_echo_area_activities? qspace : qempty,
                      $lo_echo_area_activities?
                        $q_Workspace_echo_area_meta : qempty };
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
  //$o_BufferLineEditor_cursor = object_clone($o_BufferEditor_point);
  $o_BufferLineEditor_cursor =
    within_context($o_BufferEditor_point, $c_FileBufferCursor());
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
      $($($o_BufferLineEditor_parent, $o_BufferEditor_point),
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
                        (*$q_Workspace_echo_area_meta? qspace : qempty),
                        $q_Workspace_echo_area_meta };
    $q_Workspace_echo_area_meta = qstrapv(parts, lenof(parts));
  } else {
    // On top of parent, use concise syntax
    qstring parts[] = { lparen, inner,
                        wstrtoqstr(linenum), rparen,
                        (*$q_Workspace_echo_area_meta? qspace : qempty),
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
  mwstring line = wcalloc($az_LineEditor_buffer->len + 1);
  wmemcpy(line, $az_LineEditor_buffer->v, $az_LineEditor_buffer->len);
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
    Inserts a blank line above the current line. Point will be shunted
    downward by one line as a result.
 */
defun($h_BufferEditor_insert_blank_line_above) {
  $M_edit(0, $o_BufferEditor_buffer,
          $I_FileBuffer_ndeletions = 0,
          $lw_FileBuffer_replacements = cons_w(L"", NULL),
          $I_FileBuffer_edit_line =
            $($o_BufferEditor_point, $I_FileBufferCursor_line_number));
}

/*
  SYMBOL: $f_BufferEditor_insert_blank_line_below
    Inserts a blank line below point, without advancing. If point is at
    the end of the buffer, a line is inserted before point, then
    point is retreated one line.
 */
defun($h_BufferEditor_insert_blank_line_below) {
  unsigned where = $($o_BufferEditor_point, $I_FileBufferCursor_line_number);
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
    $M_shunt(0, $o_BufferEditor_point,
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
    unsigned where = $($o_BufferEditor_point,
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
    Insert a new line before point with a BufferLineEditor. When the line
    editor is accepted, point will be shunted downward, and this method will
    be called again.

  SYMBOL: $u_continue_inserting
    Identifies hooks used to continue in "insert mode" after a line editor is
    accepted.
 */
defun($h_BufferEditor_insert_and_edit) {
  object editor = $c_BufferLineEditor(
    $i_LineEditor_point = -1,
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
    Insert a new line before point, invoke an editor on it, call
    $m_self_insert in its context. As a side-effect, point will be shunted
    downward.
    --
    This method is no longer used in the default keybindings. You can bind it
    to KEYBINDING_DEFAULT to get less modefull editing, though you'll also need
    to arrange for some way to access the unshifted BufferEditor commands which
    are currently bound to basic characters.
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
    Moves the buffer point down one line, unless already at the end of the
    file.

  SYMBOL: $I_LastCommand_forward_line
    Variable for accelerate() in $f_BufferEditor_forward_line.
 */
defun($h_BufferEditor_forward_line) {
  $$($o_BufferEditor_point) $$($o_BufferEditor_buffer) {
    $m_access();
    unsigned dist = accelerate_max(&$I_LastCommand_forward_line,
                                   $aw_FileBuffer_contents->len -
                                   $I_FileBufferCursor_line_number);
    $I_FileBufferCursor_line_number += dist;
  }

  $m_update_echo_area();
}

/*
  SYMBOL: $f_BufferEditor_forward_line_reset_mark
    Moves the buffer point down one line and resets mark.
 */
defun($h_BufferEditor_forward_line_reset_mark) {
  $m_forward_line();
  $m_reset_mark();
}

/*
  SYMBOL: $f_BufferEditor_backward_line
    Moves the buffer point up one line, unless already at the end of the
    file.

  SYMBOL: $I_LastCommand_backward_line
    Variable for accelerate() in $f_BufferEditor_backward_line.
 */
defun($h_BufferEditor_backward_line) {
  $$($o_BufferEditor_point) {
    unsigned dist = accelerate_max(&$I_LastCommand_backward_line,
                                   $I_FileBufferCursor_line_number);
    $I_FileBufferCursor_line_number -= dist;
  }

  $m_update_echo_area();
}

/*
  SYMBOL: $f_BufferEditor_backward_line_reset_mark
    Moves the buffer point up one line and resets mark.
 */
defun($h_BufferEditor_backward_line_reset_mark) {
  $m_backward_line();
  $m_reset_mark();
}

/*
  SYMBOL: $f_BufferEditor_kill_forward_line
    Kills the line in front of point, saving it to the kill ring.
 */
defun($h_BufferEditor_kill_forward_line) {
  $$($o_BufferEditor_buffer) {
    $m_access();
    $$($o_BufferEditor_point) {
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
    Kills the line behind point, saving it to the kill ring.
 */
defun($h_BufferEditor_kill_backward_line) {
  $$($o_BufferEditor_buffer) {
    $m_access();
    $$($o_BufferEditor_point) {
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
    Moves point back to the first line.
 */
defun($h_BufferEditor_home) {
  $$($o_BufferEditor_point) {
    let($i_FileBufferCursor_shunt_distance,
        -(signed)$I_FileBufferCursor_line_number);
    $m_shunt();
  }
}

/*
  SYMBOL: $f_BufferEditor_end
    Moves point forward to one line after the last line.
 */
defun($h_BufferEditor_end) {
  $$($o_BufferEditor_buffer) {
    $m_access();
    $$($o_BufferEditor_point) {
      let($i_FileBufferCursor_shunt_distance,
          $aw_FileBuffer_contents->len -
            $I_FileBufferCursor_line_number);
      $m_shunt();
    }
  }
}

/*
  SYMBOL: $f_BufferEditor_show_forward_line
    Show the line(s) below point in the current Transcript.

  SYMBOL: $I_LastCommand_show_forward_line
    Param to accelerate() for $f_BufferEditor_show_forward_line.

  SYMBOL: $I_LastCommand_show_forward_line_off
    The offset from previous calls to $f_BufferEditor_show_forward_line.
 */
defun($h_BufferEditor_show_forward_line) {
  $$($o_BufferEditor_buffer) {
    $m_access();
    $$($o_BufferEditor_point) {
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
    Shows the line(s) before point in the current Transcript.

  SYMBOL: $I_LastCommand_show_backward_line_off
    The offset from point of the last call to
    $f_BufferEditor_show_backward_line.

  SYMBOL: $I_LastCommand_show_backward_line
    Parameter to accelerate() for $f_BufferEditor_show_backward_line.
 */
defun($h_BufferEditor_show_backward_line) {
  $$($o_BufferEditor_buffer) {
    $m_access();
    $$($o_BufferEditor_point) {
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
    mqstring new = qcalloc(1+$i_column_width);
    qmemcpy(new, $q_BufferEditor_line_wrap_reverse, $i_column_width);
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
  SYMBOL: $f_BufferEditor_digit_input
    Replaces or edits the line number of point or mark according to
    $x_Terminal_input_value. Affected by
    $y_LastCommand_line_number_is_relative, $y_LastCommand_is_setting_mark,
    $i_LastCommand_relative_sign, $I_LastCommand_line_number_relative_to,
    $I_LastCommand_line_number. The "digits" a..z are interpreted as 10..35,
    and A..Z as 36..61.

  SYMBOL: $y_LastCommand_line_number_is_relative
    If true, line number entry in $f_BufferEditor_digit_input (et al) is
    relative to $I_LastCommand_line_number_relative_to.

  SYMBOL: $y_LastCommand_is_setting_mark
    If true, line number edits by $f_BufferEditor_digit_input et al affect the
    topmost mark instead of point.

  SYMBOL: $i_LastCommand_relative_sign
    When $y_LastCommand_line_number_is_relative, either +1 or -1 to indicate
    which direction relative line number entry is progressing.

  SYMBOL: $I_LastCommand_line_number_relative_to
    WHen $y_LastCommand_line_number_is_relative, indicates the base line number
    which the number being entered is relative to.

  SYMBOL: $I_LastCommand_line_number
    The magnitude of the current line number entry used by
    $f_BufferEditor_digit_input.

  SYMBOL: $y_LastCommand_was_digit_input
    True if the last command was $f_BufferEditor_digit_input.
 */
defun($h_BufferEditor_digit_input) {
  bool line_number_is_relative = 0, is_setting_mark = 0;
  signed relative_sign = 0, max = 0;
  unsigned relative_to = 0, line_number = 0;
  $$($o_prev_command) {
    line_number_is_relative = $y_LastCommand_line_number_is_relative;
    is_setting_mark = $y_LastCommand_is_setting_mark;
    relative_sign = $i_LastCommand_relative_sign;
    relative_to = $I_LastCommand_line_number_relative_to;
    line_number = $I_LastCommand_line_number;
  }

  $$($o_BufferEditor_buffer) {
    $m_access();
    max = $aw_FileBuffer_contents->len;
  }

  if (($x_Terminal_input_value < L'0' || $x_Terminal_input_value > L'9') &&
      ($x_Terminal_input_value < L'a' || $x_Terminal_input_value > L'z') &&
      ($x_Terminal_input_value < L'A' || $x_Terminal_input_value > L'Z')) {
    $y_key_dispatch_continue = true;
    return;
  }

  unsigned ones;
  if ($x_Terminal_input_value >= L'0' && $x_Terminal_input_value <= L'9')
    ones = $x_Terminal_input_value - L'0';
  else if ($x_Terminal_input_value >= L'a' && $x_Terminal_input_value <= L'z')
    ones = $x_Terminal_input_value - L'a' + 10;
  else
    ones = $x_Terminal_input_value - L'A' + 36;
    
  line_number *= 10;
  line_number += ones;

  signed real_number;
  if (line_number_is_relative)
    real_number = relative_to + relative_sign * line_number;
  else
    real_number = ((signed)line_number) - 1; //one-based indexing

  if (real_number < 0)
    real_number = 0;
  else if (real_number > max)
    real_number = max;

  // Always set the mark; either this point (if setting mark) or to one after
  // it (if setting point)
  if (!$lo_BufferEditor_marks)
    lpush_o($lo_BufferEditor_marks,
            $c_FileBufferCursor(
              $o_FileBufferCursor_buffer = $o_BufferEditor_buffer));

  $$($lo_BufferEditor_marks->car) {
    $I_FileBufferCursor_line_number =
      !is_setting_mark? (real_number == max && real_number?
                         real_number - 1 :
                         real_number == max? real_number : real_number + 1)
      : real_number;
  }
  if (!is_setting_mark) {
    $$($o_BufferEditor_point) {
      $I_FileBufferCursor_line_number = real_number;
    }
  }

  //Propagate values
  $$($o_this_command) {
    $y_LastCommand_line_number_is_relative = line_number_is_relative;
    $y_LastCommand_is_setting_mark = is_setting_mark;
    $I_LastCommand_line_number = line_number;
    $I_LastCommand_line_number_relative_to = relative_to;
    $i_LastCommand_relative_sign = relative_sign;

    $y_LastCommand_was_digit_input = true;
  }

  $m_update_echo_area();
}

/*
  SYMBOL: $f_BufferEditor_reset_mark
    Ensures that a mark exists, and that it is (if possible) one line away from
    point.
 */
defun($h_BufferEditor_reset_mark) {
  if (!$lo_BufferEditor_marks)
    lpush_o($lo_BufferEditor_marks, $c_FileBufferCursor(
              $o_FileBufferCursor_buffer = $o_BufferEditor_buffer));

  unsigned point = $($o_BufferEditor_point, $I_FileBufferCursor_line_number);
  unsigned max = 0;
  $$($o_BufferEditor_buffer) {
    $m_access();
    max = $aw_FileBuffer_contents->len;
  }

  $$($lo_BufferEditor_marks->car) {
    if (!max)
      $I_FileBufferCursor_line_number = 0;
    else if (point < max)
      $I_FileBufferCursor_line_number = point+1;
    else
      $I_FileBufferCursor_line_number = point-1;
  }

  $m_update_echo_area();
}

/*
  SYMBOL: $f_BufferEditor_move_point $I_BufferEditor_move_point_to
    Immediately moves point to the line specified by
    $I_BufferEditor_move_point_to, without resetting mark.
 */
defun($h_BufferEditor_move_point) {
  $$($o_BufferEditor_point) {
    $I_FileBufferCursor_line_number = $I_BufferEditor_move_point_to;
  }

  $m_update_echo_area();
}

/*
  SYMBOL: $f_BufferEditor_move_mark $I_BufferEditor_move_mark_to
    Immediately moves mark to the line specified by
    $I_BufferEditor_move_mark_to.
 */
defun($h_BufferEditor_move_mark) {
  if (!$lo_BufferEditor_marks) {
    lpush_o($lo_BufferEditor_marks,
            $c_FileBufferCursor(
              $o_FileBufferCursor_buffer = $o_BufferEditor_buffer,
              $I_FileBufferCursor_line_number = $I_BufferEditor_move_mark_to));
  } else {
    $$($lo_BufferEditor_marks->car) {
      $I_FileBufferCursor_line_number = $I_BufferEditor_move_mark_to;
    }
  }

  $m_update_echo_area();
}

/*
  SYMBOL: $f_BufferEditor_sign
    Enters relative line number mode, setting $i_LastCommand_relative_sign to
    $i_BufferEditor_sign. See $f_BufferEditor_digit_input().
 */
defun($h_BufferEditor_sign) {
  unsigned relative_to = 0;
  $$($o_BufferEditor_point) {
    relative_to = $I_FileBufferCursor_line_number;
  }

  $$($o_this_command) {
    $y_LastCommand_is_setting_mark =
      $($o_prev_command, $y_LastCommand_is_setting_mark) ||
      $($o_prev_command, $y_LastCommand_was_digit_input);
    $y_LastCommand_line_number_is_relative = true;
    $i_LastCommand_relative_sign = $i_BufferEditor_sign;
    $I_LastCommand_line_number_relative_to = relative_to;
    $I_LastCommand_line_number = 0;
  }
}

/*
  SYMBOL: $f_BufferEditor_sign_positive
    Calls $f_BufferEditor_sign with +1.
 */
defun($h_BufferEditor_sign_positive) {
  $i_BufferEditor_sign = +1;
  $m_sign();
}

/*
  SYMBOL: $f_BufferEditor_sign_negative
    Calls $f_BufferEditor_sign with -1.
 */
defun($h_BufferEditor_sign_negative) {
  $i_BufferEditor_sign = -1;
  $m_sign();
}

/*
  SYMBOL: $f_BufferEditor_set_mark
    Sets $y_LastCommand_is_setting_mark to true for this command, so further
    line number commands affect mark instead of point.
 */
defun($h_BufferEditor_set_mark) {
  $$($o_this_command) {
    $y_LastCommand_is_setting_mark = true;
    $I_LastCommand_line_number = 0;
    $y_LastCommand_line_number_is_relative = false;
  }
}

/*
  SYMBOL: $f_BufferEditor_print_region
    Outputs the lines between point (inclusive) and mark (exclusive) to the
    Trascript, as an output group.
 */
defun($h_BufferEditor_print_region) {
  if (!$lo_BufferEditor_marks)
    $m_reset_mark();

  unsigned point = $($o_BufferEditor_point, $I_FileBufferCursor_line_number);
  unsigned mark = $($lo_BufferEditor_marks->car,
                    $I_FileBufferCursor_line_number);

  signed start, end;
  if (point < mark)
    start = point, end = mark;
  else
    start = mark+1, end = point+1;

  $lo_BufferEditor_format = NULL;
  $$($o_BufferEditor_buffer) {
    $m_access();
    if (end > $aw_FileBuffer_contents->len)
      end = $aw_FileBuffer_contents->len;

    for (signed i = end-1; i >= start; --i) {
      $M_format(0,0, $I_BufferEditor_index = i);
    }
  }

  if ($o_Transcript)
    $M_group(0, $o_Transcript,
             $lo_Transcript_output = $lo_BufferEditor_format);
  $lo_BufferEditor_format = NULL;
}

/*
  SYMBOL: $f_BufferEditor_search $w_BufferEditor_search $i_BufferEditor_search
    Searches the buffer for $w_BufferEditor_search, beginning one line before
    or after point. If the beginning or end of the buffer is encountered, the
    search wraps. The search will stop if the line where the search began is
    encountered; this is considered a failure, even if the initial line
    matches. On success, mark is set to the line where point used to be, and
    point is set to the line which matched. On failure, point and mark are
    unchanged. Search direction is determined by $i_BufferEditor_search, which
    must be +1 or -1.
    --
    If $w_BufferEditor_search is the empty string, it is replaced by
    $w_previous_search_query.

  SYMBOL: $w_previous_search_query
    The most recent query to $f_BufferEditor_search().
 */
defun($h_BufferEditor_search) {
  if (!*$w_BufferEditor_search && $w_previous_search_query)
    $w_BufferEditor_search = $w_previous_search_query;
  else
    $w_previous_search_query = $w_BufferEditor_search;

  object pattern = $c_Pattern($w_Pattern_pattern = $w_BufferEditor_search);
  int start_line = $($o_BufferEditor_point, $I_FileBufferCursor_line_number);

  dynar_w contents;
  $$($o_BufferEditor_buffer) {
    $m_access();
    contents = $aw_FileBuffer_contents;
  }

  // Just do nothing if this buffer is empty
  if (!contents->len)
    return;

  // If starting on the virtual line at the end of the file, pretend that we
  // started one before that.
  if ($i_BufferEditor_search == contents->len)
    $i_BufferEditor_search = contents->len-1;

  int line = start_line + $i_BufferEditor_search;
  bool wrapped = false;
  while (true) {
    // Wrap if necessary
    wrapped |= (line < 0 || line >= contents->len);
    if (line < 0)
      line += contents->len;
    else
      line %= contents->len;

    if (line == start_line) {
      // Search failed
      $F_message_error(
        0, 0,
        $w_message_text = wstrap(L"Search failed: ", $w_BufferEditor_search));
      return;
    }

    if ($M_matches($y_Pattern_matches, pattern,
                   $w_Pattern_input = contents->v[line])) {
      // Success; move mark and point, and display this line
      $I_BufferEditor_move_point_to = line;
      $I_BufferEditor_move_mark_to = start_line;
      $m_move_point();
      $m_move_mark();

      //Notiy the user if the search wrapped
      if (wrapped)
        $F_message_notice(0,0,
                          $w_message_text = L"Search wrapped");

      $I_BufferEditor_index = line;
      $m_echo_line();
      return;
    }

    line += $i_BufferEditor_search;
  }
  //Will not get here
}

/*
  SYMBOL: $f_BufferEditor_search_forward $f_BufferEditor_search_forward_i
    Sets $i_BufferEditor_search to +1 and calls $f_BufferEditor_search().
 */
interactive($h_BufferEditor_search_forward_i,
            $h_BufferEditor_search_forward,
            i_(w, $w_BufferEditor_search, L"grep")) {
  $i_BufferEditor_search = +1;
  $m_search();
}

/*
  SYMBOL: $f_BufferEditor_search_backward $f_BufferEditor_search_backward_i
    Sets $i_BufferEditor_search to -1 and calls $f_BufferEditor_search().
 */
interactive($h_BufferEditor_search_backward_i,
            $h_BufferEditor_search_backward,
            i_(w, $w_BufferEditor_search, L"rgrep")) {
  $i_BufferEditor_search = -1;
  $m_search();
}

/*
  SYMBOL: $f_BufferEditor_undo
    Undoes the FileBuffer by one step.
 */
defun($h_BufferEditor_undo) {
  $M_undo(0, $o_BufferEditor_buffer);
  $m_update_echo_area();
}

/*
  SYMBOL: $f_BufferEditor_redo
    Redoes the FileBuffer by one step.
 */
defun($h_BufferEditor_redo) {
  $M_redo(0, $o_BufferEditor_buffer);
  $m_update_echo_area();
}

/*
  SYMBOL: $lp_BufferEditor_keymap
    Keybindings specific to BufferEditors.

  SYMBOL: $u_after_sign
    Key mode for the key immediately following a numeric sign command.
 */
class_keymap($c_BufferEditor, $lp_BufferEditor_keymap, $llp_Activity_keymap)
ATSINIT {
  bind_char($lp_BufferEditor_keymap, $u_ground, L'\r', NULL,
            $m_insert_blank_line_above);
  bind_char($lp_BufferEditor_keymap, $u_ground, L'o', NULL,
            $m_insert_blank_line_below);
  bind_char($lp_BufferEditor_keymap, $u_ground, L'e', NULL,
            $m_edit_current);
  bind_char($lp_BufferEditor_keymap, $u_ground, L'i', NULL,
            $m_insert_and_edit);
  bind_char($lp_BufferEditor_keymap, $u_ground, L'p', NULL,
            $m_print_region);
  bind_char($lp_BufferEditor_keymap, $u_ground, L'g', NULL,
            $m_search_forward_i);
  bind_char($lp_BufferEditor_keymap, $u_ground, L'G', NULL,
            $m_search_backward_i);

  for (wchar_t ch = L'0'; ch <= L'9'; ++ch) {
    bind_char($lp_BufferEditor_keymap, $u_ground, ch, NULL,
              $m_digit_input);
    bind_char($lp_BufferEditor_keymap, $u_after_sign, ch, $u_ground,
              $m_digit_input);
  }
  for (wchar_t ch = L'a'; ch <= L'z';  ++ch)
    bind_char($lp_BufferEditor_keymap, $u_after_sign, ch, $u_ground,
              $m_digit_input);
  for (wchar_t ch = 'A'; ch <= L'Z'; ++ch)
    bind_char($lp_BufferEditor_keymap, $u_after_sign, ch, $u_ground,
              $m_digit_input);

  bind_kp($lp_BufferEditor_keymap, $u_after_sign, KEYBINDING_DEFAULT,
          $u_ground, $m_other_input_after_sign);
  bind_char($lp_BufferEditor_keymap, $u_after_sign, '\033', $u_meta,
            NULL);

  bind_char($lp_BufferEditor_keymap, $u_ground, L'+', $u_after_sign,
            $m_sign_positive);
  bind_char($lp_BufferEditor_keymap, $u_ground, L'.', $u_after_sign,
            $m_sign_positive);
  bind_char($lp_BufferEditor_keymap, $u_ground, L'-', $u_after_sign,
            $m_sign_negative);
  bind_char($lp_BufferEditor_keymap, $u_ground, L',', $u_after_sign,
            $m_sign_negative);
  bind_char($lp_BufferEditor_keymap, $u_ground, L'/', NULL,
            $m_set_mark);

  bind_char($lp_BufferEditor_keymap, $u_extended, CONTROL_S, $u_ground,
            $m_save);

  bind_char($lp_BufferEditor_keymap, $u_meta, L'j', $v_end_meta,
            $m_backward_line_reset_mark);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'k', $v_end_meta,
            $m_forward_line_reset_mark);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'J', $v_end_meta,
            $m_backward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'K', $v_end_meta,
            $m_forward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'l', $v_end_meta,
            $m_kill_backward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L';', $v_end_meta,
            $m_kill_forward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'h', $v_end_meta,
            $m_home);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'n', $v_end_meta,
            $m_end);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'f', $v_end_meta,
            $m_show_forward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'd', $v_end_meta,
            $m_show_backward_line);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'y', $v_end_meta,
            $m_undo);
  bind_char($lp_BufferEditor_keymap, $u_meta, L'Y', $v_end_meta,
            $m_redo);
}
