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
#include "recursive_edit.slc"
#include "key_dispatch.h"

/*
  TITLE: Recursive Editting Support
  OVERVIEW: Provides key commands which allow one to break the stack ordering
    of Activities within a Workspace, and to enter a temporary recursive edit
    mode from a BufferLineEditor.
 */

/*
  SYMBOL: $f_Workspace_activity_to_bottom
    Moves the top-most Activity not marked as always-on-top to the bottom of
    this Workspace's Activity stack.
 */
defun($h_Workspace_activity_to_bottom) {
  list_o lact = find_where_o($lo_Workspace_activities,
                             lambdab((object a), !$(a, $y_Activity_on_top)));
  // If we found nothing, or there isn't anything below it, there's nothing to
  // do.
  if (!lact || !lact->cdr) return;

  // Remove it from the stack
  list_o filtered = lrmrev_o($lo_Workspace_activities, lact->car);
  // Prepend it to the bottom and reverse back to normal
  $lo_Workspace_activities = lrev_o(cons_o(lact->car, filtered));

  $m_update_echo_area();
}

/*
  SYMBOL: $f_Workspace_activity_from_bottom
    Moves the bottom-most Activity to the top of this Workspace's Activity
    stack.
 */
defun($h_Workspace_activity_from_bottom) {
  list_o last = $lo_Workspace_activities;
  if (!last || !last->cdr) return; // Only one item
  while (last->cdr) last = last->cdr;

  // Remove it
  $lo_Workspace_activities = lrm_o($lo_Workspace_activities, last->car);
  // Re-add it, so it will be as top-most as allowed
  $M_push_activity(0, last->car);

  $m_update_echo_area();
}

/*
  SYMBOL: $f_Workspace_parent_activity_to_top
    Moves the direct parent of the current Activity to the top of this
    Workspace's Activity stack.
 */
defun($h_Workspace_parent_activity_to_top) {
  if (!$lo_Workspace_activities) return;

  object act = $lo_Workspace_activities->car;
  object parent = $(act, $o_Activity_parent);
  if (!parent) return;

  $lo_Workspace_activities = lrm_o($lo_Workspace_activities, parent);
  $M_push_activity(0, parent);

  $m_update_echo_area();
}

/*
  SYMBOL: $f_Workspace_child_activity_to_top
    Moves the first direct child of the current Activity to the top of this
    Workspace's Activity stack.
 */
defun($h_Workspace_child_activity_to_top) {
  if (!$lo_Workspace_activities) return;

  object act = $lo_Workspace_activities->car;
  list_o children = $(act, $lo_Activity_children);
  if (!children) return;

  $lo_Workspace_activities = lrm_o($lo_Workspace_activities, children->car);
  $M_push_activity(0, children->car);

  $m_update_echo_area();
}

/*
  SYMBOL: $lp_recursive_edit_keymap
    Keybindings to support recursive editting by rearranging Activities.
 */
class_keymap($c_Workspace, $lp_recursive_edit_keymap, $llp_Workspace_keymap)
ATSINIT {
  bind_char($lp_recursive_edit_keymap, $u_meta, L'z', $v_end_meta,
            $m_activity_to_bottom);
  bind_char($lp_recursive_edit_keymap, $u_meta, L'Z', $v_end_meta,
            $m_activity_from_bottom);
  bind_char($lp_recursive_edit_keymap, $u_meta, L'x', $v_end_meta,
            $m_parent_activity_to_top);
  bind_char($lp_recursive_edit_keymap, $u_meta, L'X', $v_end_meta,
            $m_child_activity_to_top);
}
