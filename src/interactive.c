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
#include "interactive.slc"
#include "key_dispatch.h"

/*
  TITLE: Interactive Command Framework
  OVERVIEW: Implementation of the basic interactive command framework, as
    defined in interactive.h.
*/

object mk_interactive_obj(struct hook_point* bound) {
  object this = object_new(object_current());
  $F_Interactive(0,this, $h_Interactive_bound = *bound);
  return this;
}

/*
  SYMBOL: $c_Interactive
    Stores the information needed for an as-yet-uncompleted call to an
    interactive command.

  SYMBOL: $ao_Interactive_arguments
    An array of $c_IArg subclass objects which describe the arguments that need
    to be obtained from the user.

  SYMBOL: $i_Interactive_ix
    The index into $ao_Interactive_arguments of the next argument to obtain.

  SYMBOL: $h_Interactive_bound
    Hook to invoke once all arguments have been obtained. This hook is specific
    to each Interactive object.
 */
defun($h_Interactive) {
  $ao_Interactive_arguments = dynar_new_o();
  $i_Interactive_ix = 0;
}

/*
  SYMBOL: $c_IArg
    Base class for interactive arguments.

  SYMBOL: $H_IArg_activate
    Initialises an Activity within the current context (ie, the h reference
    to an Activity subclass constructor). The new object will have this $c_IArg
    as its parent, so any symbols defined there will be available to the new
    object as well.

  SYMBOL: $p_IArg_destination
    Pointer to argument destination.

  SYMBOL: $w_IArg_name
    Name to show as metadata for the activity.

  SYMBOL: $o_IArg_context
    The context of the Interactive command.
 */
defun($h_IArg) {
  $o_IArg_context = $o_Interactive;
}

void invoke_interactive(object this) {
  $F_invoke_interactive(0,this);
}

/*
  SYMBOL: $f_invoke_interactive
    Begins, continues, or completes a call to an interactive command as defined
    by the current context. Called by invoke_interactive(object).

  SYMBOL: $u_Interactive_continuation
    Identifies a hook to to trigger the next stage of invoking an interactive
    command.

  SYMBOL: $u_continuation
    Identifies a hook which triggers some type of continuation.

  SYMBOL: $y_IArg_abort
    If set by an Interactive-supporting Activity, the Interactive call it is a
    part of will be aborted when the Activity is destroyed.
 */
defun($h_invoke_interactive) {
  if ($y_IArg_abort) return;
  if ($i_Interactive_ix == $ao_Interactive_arguments->len) {
    // We have all arguments, call the actual function
    invoke_hook(&$h_Interactive_bound);
    return;
  }

  // If there is a current workspace, use that. Otherwise, use the workspace of
  // the current view (of the current terminal, or the first terminal), or the
  // first workspace if all else fails.
  object workspace = $o_Workspace;
  if (!workspace) {
    object view = $o_View;
    if (!view) {
      object terminal = $o_Terminal;
      if (!terminal) {
        if ($lo_terminals) {
          terminal = $lo_terminals->car;
        } else {
          // Last resort: Pick an arbitrary workspace (we're currently headless,
          // so just pick a workspace arbitrarily.)
          workspace = $lo_workspaces->car;
          goto have_workspace;
        }
      }
      view = $(terminal, $o_Terminal_current_view);
    }
    workspace = $(view, $o_View_workspace);
  }
  have_workspace:;

  // Get the next argument
  object factory = $ao_Interactive_arguments->v[$i_Interactive_ix++];
  object activity = object_new(factory);
  object this = $o_Interactive;
  $$($o_root) {
    $$(workspace) {
      $$(activity) {
        implant($d_Activity);
        $o_Activity_workspace = workspace;
        //$o_Activity here still refers to whatever activity is present, since
        //the new activity object is still bare.
        $o_Activity_parent = $o_Activity;
        invoke_hook($H_IArg_activate);
        // Set continuation up
        add_hook_obj(&$h_Activity_destroy, HOOK_AFTER,
                     $u_Interactive_continuation, $u_continuation,
                     $f_invoke_interactive, this,
                     NULL);
      }
    }
  }
}

/*
  SYMBOL: $c_IActiveActivity
    Subclass of Activity which implements functionality common to most
    Activites to be used with the Interactive system.

  SYMBOL: $q_IActiveActivity_name
    The name of this Activity, for within metadata.
 */
subclass($c_Activity, $c_IActiveActivity)
defun($h_IActiveActivity) {
  $q_IActiveActivity_name = wstrtoqstr($w_IArg_name);
  $F_Workspace_update_echo_area(0, $o_Activity_workspace);
}

/*
  SYMBOL: $f_IActiveActivity_get_echo_area_meta
    Adds the prompt for this IActiveActivity to the meta string.
 */
defun($h_IActiveActivity_get_echo_area_meta) {
  if ($lo_echo_area_activities) {
    object next = $lo_echo_area_activities->car;
    let($lo_echo_area_activities, $lo_echo_area_activities->cdr);
    $M_get_echo_area_meta(0, next);
  }
  qchar separator[2] = {L':', 0};
  $q_Workspace_echo_area_meta = qstrap3(
    $q_IActiveActivity_name, separator, $q_Workspace_echo_area_meta);
}

/*
  SYMBOL: $f_IActiveActivity_abort
    Marks the Activity as aborted, then calls $m_destroy().
 */
defun($h_IActiveActivity_abort) {
  $y_IArg_abort = true;
  $m_destroy();
}

/*
  SYMBOL: $c_WCharIArg
    Interactive Arguemnt which reads a single non-control character from the
    user.
 */
subclass($c_IArg, $c_WCharIArg)
defun($h_WCharIArg) {
  $H_IArg_activate = &$h_WCharIActive;
}

void interactive_z(wchar_t* dst, wstring prompt) {
  object iarg = $c_WCharIArg($p_IArg_destination = dst,
                              $w_IArg_name = prompt);
  dynar_push_o($ao_Interactive_arguments, iarg);
}

/*
  SYMBOL: $c_WCharIActive
    IActiveActivity which reads a single non-control character from the user.

  SYMBOL: $h_WCharIActive_char
    If $x_Terminal_input_value is a non-control character, writes to
    $p_IArg_destination (as a wchar_t*) and destroys the current Activity.

  SYMBOL: $lp_WCharIActive_keymap
    Keymap for $c_WCharIActive.
 */
subclass($c_IActiveActivity, $c_WCharIActive)
class_keymap($c_WCharIActive, $lp_WCharIActive_keymap, $llp_Activity_keymap)
defun($h_WCharIActive_char) {
  if (!is_nc_char($x_Terminal_input_value)) {
    // Not an acceptable character
    $y_key_dispatch_continue = true;
    $m_abort();
    return;
  }

  //OK
  wchar_t* dst = $p_IArg_destination;
  wchar_t val = $x_Terminal_input_value;
  $$($o_IArg_context) {
    *dst = val;
  }
  $m_destroy();
}

ATSTART(ANONYMOUS, STATIC_INITIALISATION_PRIORITY) {
  bind_kp($lp_WCharIActive_keymap, $u_ground, KEYBINDING_DEFAULT, NULL,
          $f_WCharIActive_char);
}

/*
  SYMBOL: $c_WStringIArg
    Interactive Argument which reads an arbitrary string from the user.

  SYMBOL: $w_WStringIArg_text
    If non-NULL, the initial text to give to the user.

  SYMBOL: $H_WStringIArg_validate
    If non-NULL, will be called with $w_WStringIArg_validate set to the string
    the user wants to use. It may reject the string by setting
    $w_WStringIArg_why_not to a non-NULL which should tell the user why the
    input is not acceptable.

  SYMBOL: $w_WStringIArg_why_not
    May be set by $H_WStringIArg_validate to indicate that and why the user's
    input (in $w_WStringIArg_validate) is unacceptable.
 */
subclass($c_IArg, $c_WStringIArg)
defun($h_WStringIArg) {
  $H_IArg_activate = &$h_WStringIActive;
}

void interactive_w(wstring* dst, wstring prompt) {
  object iarg = $c_WStringIArg($p_IArg_destination = dst,
                               $w_IArg_name = prompt);
  dynar_push_o($ao_Interactive_arguments, iarg);
}

subclass($c_LineEditor, $c_WStringIActive)
subclass($c_IActiveActivity, $c_WStringIActive)
advise_before_superconstructor($h_WStringIActive) {
  if ($w_WStringIArg_text)
    $w_LineEditor_text = $w_WStringIArg_text;
}

/*
  SYMBOL: $f_WStringIActive_accept
    Validates the user's input (if applicable), ther writes to the destination
    and destroys this activity.
 */
defun($h_WStringIActive_accept) {
  $w_WStringIArg_text = $M_get_text($w_LineEditor_text, 0);
  if ($H_WStringIArg_validate) {
    $w_WStringIArg_why_not = NULL;
    $$($o_IArg_context) {
      invoke_hook($H_WStringIArg_validate);
    }
    if ($w_WStringIArg_why_not) {
      $F_message_error(0,0, $w_message_text = $w_WStringIArg_why_not);
      return;
    }
  }

  wstring* dst = $p_IArg_destination;
  wstring val = $w_WStringIArg_text;
  $$($o_IArg_context) {
    *dst = val;
  }
  $m_destroy();
}

/*
  SYMBOL: $c_WStringIActive
    IActiveActivity which reads an arbitrary string from the user.
 */

unsigned accelerate(unsigned* var) {
  unsigned speed = 0;
  $$($o_prev_command) {
    speed = *var;
  }
  $$($o_this_command) {
    *var = speed+1;
  }

  return speed ?: 1;
}

unsigned accelerate_max(unsigned* var, unsigned max) {
  unsigned speed = accelerate(var);
  return speed > max? max : speed;
}
