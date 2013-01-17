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
#include "key_dispatch.slc"

//class $c_Terminal $c_View $c_Workspace $c_Backing $c_Activity

/*
  TITLE: Key Dispatch Handler
  OVERVIEW: Implements the keybinding dispatcher as described in the header
    file.
*/

/*
  SYMBOL: $v_Terminal_key_mode
    The identity of the current key mode for the containing Terminal.

  SYMBOL: $u_ground
    The "ground state" for key modes. This is the default, top-level mode.
*/
STATIC_INIT_TO($v_Terminal_key_mode, $u_ground)

static bool search(list_lp, qchar);
static bool search_all(qchar);

/*
  SYMBOL: $u_key_dispatch
    Identifies the advice the key dispatcher places on $h_Terminal_getch.

  SYMBOL: $f_Terminal_getch
    Advised by the key dispatcher to convert keystrokes into actions.

  SYMBOL: $f_key_undefined
    Called when a key is pressed which does not have any known mapping.
 */
advise_id($u_key_dispatch, $h_Terminal_getch) {
  if (!search_all($i_Terminal_input_value))
    if (!search_all(KEYBINDING_DEFAULT))
      $f_key_undefined();
}

/*
  SYMBOL: $llp_Terminal_keymap
    A list of lists of keybinding*s defining the keybindings for the current
    Terminal.

  SYMBOL: $llp_View_keymap
    A list of lists of keybinding*s defining the keybindings for the current
    View.

  SYMBOL: $llp_Workspace_keymap
    A list of lists of keybinding*s defining the keybindings for the current
    Workspace.

  SYMBOL: $llp_Backing_keymap
    A list of lists of keybinding*s defining the keybindings for the current
    Workspace Backing.

  SYMBOL: $llp_Activity_keymap
    A list of lists of keybinding*s defining the keybindings for the containing
    Activity.
 */
static bool search_all(qchar key) {
  if (!search($llp_Terminal_keymap, key)) {
    $$($o_Terminal_current_view) {
      if (!search($llp_View_keymap, key)) {
        $$($o_View_workspace) {
          if (!search($llp_Workspace_keymap, key)) {
            bool found;
            $$($o_Workspace_backing) {
              found = search($llp_Backing_keymap, key);
            }
            if (!found) {
              for (list_o curr = $lo_Workspace_activities; curr;
                   curr = curr->cdr) {
                $$(curr->car) {
                  if (search($llp_Activity_keymap, key))
                    return true;
                }
              }
            } else {
              return true;
            }
          } else {
            return true;
          }
        }
      } else {
        return true;
      }
    }
  } else {
    return true;
  }

  // If we get here, the keybinding was not found.
  return false;
}

/*
  SYMBOL: $y_key_dispatch_continue
    If set to be true by a keybinding function, searching will continue as if
    the keybinding did not exist. Note that even in this case, the key mode
    will be changed to what the keybinding requests if not NULL, so mode
    switches should not be combined with functions which may potentially set
    this to true.
 */
static bool search(list_lp llst, qchar key) {
  for (list_lp llcurr = llst; llcurr; llcurr = llcurr->cdr) {
    for (list_p lcurr = llcurr->car; lcurr; lcurr = lcurr->cdr) {
      const keybinding* kb = lcurr->car;
      if (key == kb->trigger &&
          (!kb->mode || kb->mode == $v_Terminal_key_mode)) {
        $y_key_dispatch_continue = false;
        if (kb->function)
          kb->function();
        if (kb->newmode)
          $v_Terminal_key_mode = kb->newmode;

        if (!$y_key_dispatch_continue)
          return true;
      }
    }
  }

  return false;
}

keybinding* mk_keybinding(qchar qch, identity mode, identity newmode,
                          void (*fun)(void)) {
  keybinding kb = {
    .trigger = qch,
    .mode = mode,
    .newmode = newmode,
    .function = fun,
  };
  return newdup(&kb);
}
