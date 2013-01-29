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
#ifndef INTERACTIVE_H_
#define INTERACTIVE_H_

/**
 * Creates an interactive/programatic function pair. The interactive funtion is
 * named _name_, and will obtain user input as requested by the specs in the
 * tail arguments. Once all arguments are obtained, the programatic function
 * named _bound_ will be called, which will be in the same context as where the
 * interactive function was invoked, save for an extra object which stores the
 * interactive parameters.
 *
 * Both name and bound must be $h symbols.
 *
 * Example:
 *   interactive($h_insert_n_chars_i, $h_insert_n_chars,
 *               i_(z, $z_ch, L"Character"), i_(I,$I_cnt, L"Count")) {
 *     ...
 *   }
 *
 * The i_() marco below handles the most generic cases of interactive input. It
 * is possible to easily extend the interactive system; simply define a macro
 * which takes a symbol (and whatever else you need), implants the symbol into
 * the current context, and registers the argument into
 * $ao_Interactive_arguments (which should be done by calling a function
 * declared in your header, since Silc won't generate that symbol in the user's
 * compilation unit unless they reference it themselves).
 */
#define interactive(name, bound, ...)                   \
  defun(name) {                                         \
    object iactive = mk_interactive_obj(&bound);        \
    $$(iactive) {                                       \
      __VA_ARGS__;                                      \
    }                                                   \
    invoke_interactive(iactive);                        \
  }                                                     \
  defun(bound)

/**
 * Creates and returns an object suitable for use by the interactive() macro.
 */
object mk_interactive_obj(struct hook_point*);

/**
 * Begins invoking an interactive function, using iactive to store arguments,
 * and eventually calling the bound function once all are obtained.
 */
void invoke_interactive(object iactive);

/**
 * Defines a single interactive parameter. _type_ is the type of the symbol
 * (ie, what is between the $ and the _), which must be one of I, w, i, x, y,
 * or z. _dst_ is a symbol of that type. _prompt_ is the prompt to show to the
 * user, a wide string.
 */
#define i_(type, sym, prompt) ({                \
      implant(sym);                             \
      interactive_##type(&sym, prompt);         \
    })

/**
 * On successive calls with the same pointer, returns the sequence 1, 1, 2, 3,
 * 4, ... . This uses $o_this_command and $o_prev_command to track position in
 * the sequence, so the pointer should point to a variable implanted in
 * $c_LastCommand.
 */
unsigned accelerate(unsigned*);

/**
 * Calls accelerate() with the same pointer, but limits the return value to the
 * given maximum (inclusive).
 */
unsigned accelerate_max(unsigned*, unsigned);

void interactive_I(unsigned*, wstring);
void interactive_w(wstring*, wstring);
void interactive_i(signed*, wstring);
void interactive_y(bool*, wstring);
void interactive_x(qchar*, wstring);
void interactive_z(wchar_t*, wstring);

#endif /* INTERACTIVE_H_ */
