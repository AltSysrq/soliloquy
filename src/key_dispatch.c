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
#include "inc_ncurses.h"

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

/*
  SYMBOL: $c_LastCommand
    Used to contain booleans indicating the type of the most recent keyboard
    command run. The only two relevant instances are $o_this_command and
    $o_prev_command.

  SYMBOL: $o_this_command
    An instance of $c_LastCommand which is reinitialised on every
    keystroke. Commands that need to remember that they were run should set
    variables within when the are run.

  SYMBOL: $o_prev_command
    The $o_this_command from the previous keystroke.
 */
STATIC_INIT_TO($o_this_command, $c_LastCommand())
STATIC_INIT_TO($o_prev_command, $c_LastCommand())

static bool search_one(list_lp, qchar);
static bool search_all(qchar);
static bool search(list_lp, qchar);

/*
  SYMBOL: $u_key_dispatch
    Identifies the advice the key dispatcher places on $h_Terminal_getch.

  SYMBOL: $f_Terminal_getch
    Advised by the key dispatcher to convert keystrokes into actions.

  SYMBOL: $f_key_undefined
    Called when a key is pressed which does not have any known mapping.
 */
advise_id($u_key_dispatch, $h_Terminal_getch) {
  $o_prev_command = $o_this_command;
  $o_this_command = $c_LastCommand();
  if (!search_all($x_Terminal_input_value))
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
  $$($($($o_Terminal_current_view,$o_View_workspace),$o_Workspace_backing)) {
    if (!search($llp_Terminal_keymap, key)) {
      $$($o_Terminal_current_view) {
        if (!search($llp_View_keymap, key)) {
          $$($o_View_workspace) {
            if (!search($llp_Workspace_keymap, key)) {
              if (!search($llp_Backing_keymap, key)) {
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
  }

  // If we get here, the keybinding was not found.
  return false;
}

static bool search(list_lp list, qchar key) {
  return search_one(list,key) || search_one(list,KEYBINDING_DEFAULT);
}

/*
  SYMBOL: $y_key_dispatch_continue
    If set to be true by a keybinding function, searching will continue as if
    the keybinding did not exist. Note that even in this case, the key mode
    will be changed to what the keybinding requests if not NULL, so mode
    switches should not be combined with functions which may potentially set
    this to true.
 */
static bool search_one(list_lp llst, qchar key) {
  for (list_lp llcurr = llst; llcurr; llcurr = llcurr->cdr) {
    for (list_p lcurr = llcurr->car; lcurr; lcurr = lcurr->cdr) {
      const keybinding* kb = lcurr->car;
      if (key == kb->trigger &&
          (!kb->mode || kb->mode == $v_Terminal_key_mode)) {
        $y_key_dispatch_continue = false;
        if (kb->function) {
          __label__ error;
          void on_rollback(void) {
            goto error;
          }
          tx_start(on_rollback);
          kb->function();
          tx_commit();
          if (false) {
            error:;
            unsigned len = mbstowcs(NULL, $s_rollback_reason, 0);
            wchar_t str[len+1];
            str[len] = 0;
            mbstowcs(str, $s_rollback_reason, len);
            let($w_message_text, str);
            $f_message_error();
          }
        } else {
          // Nothing is being run; move the previous command back to the
          // current
          $o_this_command = $o_prev_command;
        }
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

/*
  SYMBOL: $lp_mode_names
    A indentity-wstring map which names the values stored in
    $v_Terminal_key_mode.
 */
ATSINIT {
  $lp_mode_names =
    lmput_p(
      lmput_p(
        lmput_p(
          $lp_mode_names,
          $u_meta, L"M-"),
        $u_extended, L"^X "),
      $u_extended_meta, L"^X M-");
}

defun($h_key_undefined) {
  list_p lprefix = lmget_p($lp_mode_names,
                           $v_Terminal_key_mode);
  wstring prefix = (lprefix? lprefix->car : L"");

  wstring wkeyname;

  if ($x_Terminal_input_value & (1<<31)) {
    wkeyname = cstrtowstr(keyname($x_Terminal_input_value &~ (1<<31)));
  } else if ($x_Terminal_input_value == CONTROL_SPACE) {
    wkeyname = L"^SPC";
  } else if ($x_Terminal_input_value == L'\r') {
    wkeyname = L"RET";
  } else if ($x_Terminal_input_value < L' ') {
    static wchar_t str[3] = L"^X";
    str[1] = $x_Terminal_input_value + L'@';
    wkeyname = str;
  } else if ($x_Terminal_input_value == L' ') {
    wkeyname = L"SPC";
  } else if ($x_Terminal_input_value == 0x7F) {
    wkeyname = L"^?";
  } else {
    static wchar_t str[2] = L"X";
    str[0] = $x_Terminal_input_value;
    wkeyname = str;
  }

  static wchar_t message[256];
  swprintf(message, lenof(message), L"Undefined: %ls%ls",
           prefix, wkeyname);

  $$($o_Terminal_current_view) {
    $$($o_View_workspace) {
      $$($o_Workspace_backing) {
        $F_message_error(0,0, $w_message_text = message);
      }
    }
  }

  $v_Terminal_key_mode = $u_ground;
}
