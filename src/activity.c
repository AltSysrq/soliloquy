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
#include "activity.slc"
#include "key_dispatch.h"

/*
  TITLE: Activity Abstract Class
  OVERVIEW: Basic management functions for workspace Activities.
*/

/*
  SYMBOL: $c_Activity
    An Activity abstractly represents what the user is currently doing on a
    Workspace. Most commands run in an Activity context. The constructor
    automatically adds the Activity to the front of its Workspace.

  SYMBOL: $o_Activity_workspace
    The workspace on which this Activity is located.
 */
defun($h_Activity) {
  $$($o_Activity_workspace) {
    lpush_o($lo_Workspace_activities, $o_Activity);
  }
}

/*
  SYMBOL: $f_Activity_destroy
    Destroys this Activity, removing it from its Workspace's Activities list.
 */
defun($h_Activity_destroy) {
  $$($o_Activity_workspace) {
    $lo_Workspace_activities = lrm_o($lo_Workspace_activities, $o_Activity);
    $m_update_echo_area();
  }
}

/*
  SYMBOL: $lp_Activity_base_keymap
    The basic keymap applied to all Activities.
 */
class_keymap($c_Activity, $lp_Activity_base_keymap, $llp_Activity_keymap)
ATSINIT {
  bind_char($lp_Activity_base_keymap, $u_ground, CONTROL_G, NULL,
            $m_abort);
}
