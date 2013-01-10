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
#include "view.slc"

/*
  TITLE: Terminal/Workspace View Management
  OVERVIEW: Pairs Terminals wtih Workspaces, tracking the location of cut (both
    within the workspace and on the screen).
*/

//class $c_Terminal

/*
  SYMBOL: $c_View
    Binds workspace-specific data for each workspace to a Terminal.

  SYMBOL: $i_View_cut_on_screen
    The line location of cut on the Terminal. 0 is the top of the leftmost
    text column.

  SYMBOL: $i_View_cut_in_workspace
    The index of the last line in the workspace backing.

  SYMBOL: $u_View_Backing_change_notify
    Identity of the change notification hook used by View, to find out when
    portions of the view must be updated.

  SYMBOL: $o_View_workspace
    The workspace bound to this View.
*/
defun($h_View) {
  $i_View_cut_on_screen = 0;
  $i_View_cut_in_workspace = $($($o_Workspace, $o_Workspace_backing),
                               $ao_Backing_lines)->len;
  $o_View_workspace = $o_Workspace;
  within_context($($o_View_workspace,$o_Workspace_backing),
                 ({
                   add_hook_obj(&$h_Backing_alter, HOOK_AFTER,
                                $u_View_Backing_change_notify,
                                $u_View,
                                $f_View_backing_changed,
                                $o_View, NULL);
                 }));
  within_context($o_View_workspace,
                 ({
                   add_hook_obj(&$h_Workspace_pin_changed,
                                HOOK_AFTER,
                                $u_View_pin_change_notify,
                                $u_View,
                                $f_View_pin_changed,
                                $o_View, NULL);
                   add_hook_obj(&$h_Workspace_destroy,
                                HOOK_BEFORE,
                                $u_View,
                                $u_View,
                                $f_View_destroy,
                                $o_View, NULL);
                 }));
}

/*
  SYMBOL: $f_View_destroy
    Releases references to the subordinate Workspace of this View, and deletes
    its hook thereinto.
*/
defun($h_View_destroy) {
  within_context($($o_View_workspace,$o_Workspace_backing),
                 ({
                   del_hook(&$h_Backing_alter,
                            HOOK_AFTER,
                            $u_View_Backing_change_notify,
                            $o_View);
                 }));
  within_context($o_View_workspace,
                 ({
                   del_hook(&$h_Workspace_pin_changed,
                            HOOK_AFTER,
                            $u_View_pin_change_notify,
                            $o_View);
                   del_hook(&$h_Workspace_destroy,
                            HOOK_BEFORE,
                            $u_View,
                            $o_View);
                 }));
  $o_View_workspace = NULL;
}
