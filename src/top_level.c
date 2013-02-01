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
#include "top_level.slc"
#include "interactive.h"
#include "key_dispatch.h"

/*
  SYMBOL: $c_TopLevel
    The "top-level" activity which manages BufferEditors and such.

  SYMBOL: $o_TopLevel_curr_buffer
    The FileBuffer to make the current buffer, or which currently is the
    current buffer.

  SYMBOL: $u_registration
    Identifies hooks by class which perform additional (de)registration
    operations.
 */
subclass($c_Activity, $c_TopLevel)
defun($h_TopLevel) {
  // We need to modify the BufferEditor class to track the BufferEditors local
  // to this TopLevel
  implant($h_BufferEditor);
  implant($h_BufferEditor_destroy);
  add_hook_obj(&$h_BufferEditor, HOOK_AFTER,
               $u_registration, $u_BufferEditor,
               $m_register_buffer, $o_TopLevel,
               NULL);
  add_hook_obj(&$h_BufferEditor_destroy, HOOK_BEFORE,
               $u_registration, $u_BufferEditor,
               $m_deregister_buffer, $o_TopLevel,
               NULL);
               
  // Activate whatever buffer is first in the list
  $M_activate(0,0,
              $o_TopLevel_curr_buffer = $lo_buffers->car);
}

/*
  SYMBOL: $f_TopLevel_register_buffer
    Registers $o_BufferEditor into this TopLevel, creating a
    FileBuffer-BufferEditor mapping as necessary.

  SYMBOL: $lo_TopLevel_editors
    A FileBuffer-BufferEditor mapping of the editors currently known to this
    TopLevel.
 */
defun($h_TopLevel_register_buffer) {
  $lo_TopLevel_editors = lmput_o($lo_TopLevel_editors,
                                 $($o_BufferEditor,
                                   $o_BufferEditor_buffer),
                                 $o_BufferEditor);
}

/*
  SYMBOL: $f_TopLevel_deregister_buffer
    Deregisters $o_BufferEditor from this TopLevel, removing its
    FileBuffer-BufferEditor as necessary, and selecting a different buffer if
    this was the current buffer.
 */
defun($h_TopLevel_deregister_buffer) {
  object buffer = $($o_BufferEditor, $o_BufferEditor_buffer);
  $lo_TopLevel_editors = lmdel_o($lo_TopLevel_editors, buffer);

  if (buffer == $o_TopLevel_curr_buffer) {
    // There must always be at least one buffer; if one is being closed, we may
    // safely assume that there are currently at least two.
    $o_TopLevel_curr_buffer =
      (buffer == $lo_buffers->car?
       $lo_buffers->cdr->car : $lo_buffers->car);
    $M_activate(0, $o_TopLevel);
  }
}

/*
  SYMBOL: $f_TopLevel_activate
    Selects or creates a BufferEditor for $o_TopLevel_curr_buffer, setting
    $o_TopLevel_curr_editor.

  SYMBOL: $o_TopLevel_curr_editor
    Maintains the current editor of this TopLevel.
 */
defun($h_TopLevel_activate) {
  if ($o_TopLevel_curr_editor)
    $lo_Workspace_activities = lrm_o($lo_Workspace_activities,
                                     $o_TopLevel_curr_editor);

  list_o curr = lmget_o($lo_TopLevel_editors, $o_TopLevel_curr_buffer);
  if (curr) {
    lpush_o($lo_Workspace_activities, curr->cdr->car);
  } else {
    $o_TopLevel_curr_editor =
      $c_BufferEditor($o_BufferEditor_buffer = $o_TopLevel_curr_buffer);
    $lo_TopLevel_editors = lmput_o($lo_TopLevel_editors,
                                   $o_TopLevel_curr_buffer,
                                   $o_TopLevel_curr_editor);
  }

  $m_update_echo_area();
}

/*
  SYMBOL: $f_TopLevel_visit_file $f_TopLevel_visit_file_i
    Opens a file whose name is obtained from the user (stored in
    $w_TopLevel_filename). If a buffer for that file already exists, it is
    activated instead.

  SYMBOL: $w_TopLevel_filename
    The file to visit in a call to $f_TopLevel_visit_file
 */
interactive($h_TopLevel_visit_file_i,
            $h_TopLevel_visit_file,
            i_(w, $w_TopLevel_filename, L"Visit File")) {
  // See if there is already such a buffer
  list_o lexisting =
    find_where_o(
      $lo_buffers,
      lambda((object o),
             (bool)!wcscmp($w_TopLevel_filename,
                           $(o, $w_FileBuffer_filename))));
  $o_TopLevel_curr_buffer =
    lexisting? lexisting->car : NULL;

  if (!$o_TopLevel_curr_buffer) {
    $o_TopLevel_curr_buffer =
      $c_FileBuffer($w_FileBuffer_filename = $w_TopLevel_filename,
                    $y_FileBuffer_memory_backed = false);
  }

  $m_activate();
}

/*
  SYMBOL: $lp_TopLevel_keymap
    Keybindings specific to the TopLevel Activity.
 */
class_keymap($c_TopLevel, $lp_TopLevel_keymap, $llp_Activity_keymap)
ATSINIT {
  bind_char($lp_TopLevel_keymap, $u_extended, CONTROL_F, $u_ground,
            $m_visit_file_i);
}
