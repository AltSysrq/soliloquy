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
#include "workspace.slc"

/*
  TITLE: Workspace Management
  OVERVIEW: Manages Workspaces, global objects associated with a Backing, an
  Echo area, and an activity stack.
*/
//class $c_Backing $c_Clip

/*
  SYMBOL: $c_Workspace
    Creates a new Workspace bound to the backing in $o_Workspace_backing. The
    Workspace picks the lowest available workspace number and adds itself to
    the global workspace list.

  SYMBOL: $lo_Workspace_activities
    The current activity stack for this Workspace. The current activity is the
    entry on the top of the stack.

  SYMBOL: $o_Backing_default_activity
    The default activity for a given Backing. If this is not set for a
    particular Backing, you won't be able to do much with a Workspace backed by
    it.

  SYMBOL: $lo_Workspace_pins
    A list of Clips which are pinned to this Workspace.

  SYMBOL: $i_Workspace_number
    The number of this Workspace, which generally will not change once the
    Workspace is created (though it can be changed). The first Workspace number
    is zero; when each Workspace is constructed, it chooses the lowest
    available Workspace number.

  SYMBOL: $lo_workspaces
    A list of all existing Workspaces.

  SYMBOL: $o_Workspace_backing
    The Backing object which stores the contents of this Workspace.
 */
defun($h_Workspace) {
//  $lo_Workspace_activities =
//    cons_o($($o_Workspace_backing, $o_Backing_default_activity), NULL);
  $lo_Workspace_pins = NULL;

  // Assign a workspace number
  if ($$li_free_workspace_numbers) {
    // Use the lowest available integer
    int minimum = 0x7FFFFFFF;
    for (list_i i = $$li_free_workspace_numbers; i; i = i->cdr)
      if (i->car < minimum)
        minimum = i->car;

    $i_Workspace_number = minimum;
    $$li_free_workspace_numbers =
      lrm_i($$li_free_workspace_numbers, $i_Workspace_number);
  } else {
    // Create a new number
    $i_Workspace_number = $$i_next_new_workspace_number++;
  }

  $lo_workspaces = cons_o($o_Workspace, $lo_workspaces);
}

/*
  SYMBOL: $f_Workspace_destroy
    Destroys this workspace, returns its number to the pool, and destroys all
    workspace pins.
*/
defun($h_Workspace_destroy) {
  $$li_free_workspace_numbers = cons_i($i_Workspace_number,
                                       $$li_free_workspace_numbers);
  $lo_workspaces = lrm_o($lo_workspaces, $o_Workspace);

  each_o($lo_Workspace_pins, lambdav((object o), $M_destroy(0,o)));
  each_o($lo_Workspace_activities, lambdav((object o), $M_destroy(0,o)));
}

/*
  SYMBOL: $f_Workspace_add_pin
    Adds the current Clip ($o_Clip) to this Workspace's pin list. After this
    call, the Clip belongs to the Workspace, and will be destroyed when the
    Workspace is destroyed.

  SYMBOL: $f_Workspace_pin_changed
    Called whenever any modification to the Workspace's pin list occurs.

  SYMBOL: $f_Workspace_remove_pin
    Removes $o_Clip from this Workspace's pin list. The Clip is not destroyed;
    it becomes the caller's responsibility after this call. This should be
    called within the context of the clip, so that no long-term reference to it
    is created.

  SYMBOL: $f_Workspace_destroy_pin
    Calls $f_Workspace_remove_pin, then destroys $o_Clip. This must be called
    within the context of $o_Clip. This is both due to how it calls
    $f_Clip_destroy(), and because this prevents passing the Clip in a manner
    which would preserve a reference to it after the call exits.
 */
defun($h_Workspace_add_pin) {
  $lo_Workspace_pins = cons_o($o_Clip, $lo_Workspace_pins);
  $f_Workspace_pin_changed();
}

defun($h_Workspace_remove_pin) {
  $lo_Workspace_pins = lrm_o($lo_Workspace_pins, $o_Clip);
  $f_Workspace_pin_changed();
}

defun($h_Workspace_destroy_pin) {
  $f_Workspace_remove_pin();
  $m_destroy();
}

static list_o push_activity(list_o list) {
  // Push here if the list is empty, this activity is "on top", or the topmost
  // activity is not on top.
  if (!list || $y_Activity_on_top ||
      !$(list->car, $y_Activity_on_top))
    return cons_o($o_Activity, list);

  //Otherwise, the new activity must go under the topmost
  return cons_o(list->car, push_activity(list->cdr));
}

/*
  SYMBOL: $f_Workspace_push_activity
    Pushes the activity which is the given context onto this Workspace's
    Activity list, respecing $y_Activity_on_top.

  SYMBOL: $y_Activity_on_top
    Activities with this bool set will always be before those for which it is
    clear in $lo_Workspace_activities.
 */
defun($h_Workspace_push_activity) {
  $lo_Workspace_activities = push_activity($lo_Workspace_activities);
}
