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
#ifndef KEY_DISPATCH_H_
#define KEY_DISPATCH_H_

/**
 * This file defines the basic interface to the key dispatch system.
 *
 * At any given time, there is a current "key mode" ($v_Terminal_key_mode)
 * which affects which keybindings have effect. They are indended for meta keys
 * and key sequences like the C-x bindings in Emacs, though they can also be
 * used to make the editor modal like Vi.
 *
 * Each time the user presses a key, the following keymaps are searched:
 *   $llp_Terminal_keymap (in the terminal on which the key press occurred)
 *   $llp_View_keymap (in $o_Terminal_view)
 *   $llp_Workspace_keymap (in $o_View_workspace)
 *   $llp_Backing_keymap (in $o_Workspace_backing)
 *   $llp_Activity_keymap (from top to bottom of $lo_Workspace_activities)
 *
 * Each keymap is a list of lists of pointers to keybinding structures. The
 * two-dimensionality of the list is only to ease management of keymaps; they
 * are effectively flattened when searched.
 *
 * Searching is performed in two passes for each level of searching. On the
 * first pass, the exact keystroke ($x_Terminal_input_value) is compared
 * against keybindings' triggers, respecting mode. If no such keybinding is
 * found, the search is repeated searching as if KEYBINDING_DEFAULT had been
 * typed. If that search is also unsuccessful, $f_key_undefined() is called.
 *
 * Handler functions are called within the context of the object whose keymap
 * it was found within, as well as within the contexts of the objects searched
 * first. The exception is Activities, for which only the Activity which the
 * keymap was found in will be eviscerated, in addition to the Workspace and
 * everything above it (versus all Activities thereto and the Backing being
 * eviscerated as well).
 *
 * Any handling function may set $y_key_dispatch_continue to true to indicate
 * that it has not processed the keystroke, which causes the search to continue
 * as if that keybinding were not present in the keymap.
 */

/**
 * Defines a single keybinding (a mapping from a keystroke to an action).
 */
typedef struct {
  /**
   * The character which triggers this keybinding. Bit 31 is set to indicate
   * ncurses virtual keypad keys, or KEYBINDING_DEFAULT.
   */
  qchar trigger;
  /**
   * The key mode that this binding applies to. $u_ground is the default
   * mode. A mode of NULL is a pervasive keybinding -- it applies in ALL
   * modes. Such bindings should be used sparingly.
   */
  identity mode;
  /**
   * The mode to transition into after executing this keybinding. NULL means to
   * stay in the current mode.
   */
  identity newmode;
  /**
   * The function to execute when this keybinding is triggered. NULL indicates
   * to take no special action.
   */
  void (*function)(void);
} keybinding;

/**
 * Virtual key character which indicates a command to execute when no other
 * keybinding applies. It is intended to provide a self-insert-like
 * functionality as in Emacs.
 */
#define KEYBINDING_DEFAULT ((qchar)0x80000000)

// Control keys.
// Note that C-/ is the same as C-_.
#define CONTROL_SPACE     ((qchar)0x00)
#define CONTROL_A         ((qchar)0x01)
#define CONTROL_B         ((qchar)0x02)
#define CONTROL_C         ((qchar)0x03)
#define CONTROL_D         ((qchar)0x04)
#define CONTROL_E         ((qchar)0x05)
#define CONTROL_F         ((qchar)0x06)
#define CONTROL_G         ((qchar)0x07)
#define CONTROL_H         ((qchar)0x08)
#define CONTROL_I         ((qchar)0x09)
#define CONTROL_J         ((qchar)0x0A)
#define CONTROL_K         ((qchar)0x0B)
#define CONTROL_L         ((qchar)0x0C)
#define CONTROL_M         ((qchar)0x0D)
#define CONTROL_N         ((qchar)0x0E)
#define CONTROL_O         ((qchar)0x0F)
#define CONTROL_P         ((qchar)0x10)
#define CONTROL_Q         ((qchar)0x11)
#define CONTROL_R         ((qchar)0x12)
#define CONTROL_S         ((qchar)0x13)
#define CONTROL_T         ((qchar)0x14)
#define CONTROL_U         ((qchar)0x15)
#define CONTROL_V         ((qchar)0x16)
#define CONTROL_W         ((qchar)0x17)
#define CONTROL_X         ((qchar)0x18)
#define CONTROL_Y         ((qchar)0x19)
#define CONTROL_Z         ((qchar)0x1A)
#define ESCAPE            ((qchar)0x1B)
#define CONTROL_BACKSLASH ((qchar)0x1C)
#define CONTROL_RBRACK    ((qchar)0x1D)
#define CONTROL_CIRCUM    ((qchar)0x1E)
#define CONTROL_SLASH     ((qchar)0x1F)
#define CONTROL_USCORE    ((qchar)0x1F)

/**
 * GC-allocs a new keybinding, initialises it with the given parameters, and
 * returns it.
 */
keybinding* mk_keybinding(qchar, identity, identity, void (*)(void))
__attribute__((malloc));

/**
 * Prepends the given character to the given (lp-type) list.
 *
 * Example:
 *   bind_char($lp_my_keymap, $u_ground, CONTROL_A, 0, $f_do_something);
 */
#define bind_char(list, mode, character, newmode, fun)     \
  (list = cons_p(mk_keybinding((qchar)(character), mode, newmode, fun), list))
/**
 * Prepends the given ncurses keypad key to the given (lp-type) list. kp should
 * be a KEY_* symbol defined in curses.h (see getch(3ncurses)).
 *
 * Example:
 *   bind_kp($lp_my_keymap, $u_ground, KEY_BTAB, 0, $f_unindent);
 */
#define bind_kp(list, mode, kp, newmode, fun)    \
  bind_char(list, mode, ((qchar)0x80000000)|((qchar)(kp)), newmode, fun)

/**
 * Advises the constructor of the named class to prepend a keybinding list to
 * one of the list-lists after construction. This should be used to create
 * keybindings specific to a certain subclass, for example.
 *
 * Example:
 *   class_keymap($c_Some_Editor, $lp_Some_Editor_keymap, $llp_Backing_keymap);
 */
#define class_keymap(class, list, listlist) \
  advise_after(_GLUE(class,$hook)) {        \
    listlist = cons_lp(list, listlist);     \
  }

#endif /* KEY_DISPATCH_H_ */
