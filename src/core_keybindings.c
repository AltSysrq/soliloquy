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
#include "core_keybindings.slc"

#include "key_dispatch.h"

/*
  TITLE: Core Keybindings
  OVERVIEW: Sets up the basic keybindings for things like the handling of the
    escape key and such.
*/

/*
  SYMBOL: $u_meta
    The key mode used after escape has been pressed in the ground state (most
    commonly, this occurs due to holding Meta with the key, which sends escape
    followed by another key).

  SYMBOL: $u_extended
    Key mode for extended mnemonic commands. By default, entered by pressing ^X.

  SYMBOL: $u_extended_meta
    Key mode when Escape was read while in the $u_extended key mode. See also
    $u_meta.
 */
ATSTART(setup_core_keybindings, STATIC_INITIALISATION_PRIORITY) {
  bind_char($$lp_core_keybindings, $u_ground, L'\033', $u_meta, NULL);
  bind_char($$lp_core_keybindings, $u_ground, CONTROL_X, $u_extended, NULL);
  bind_char($$lp_core_keybindings, $u_extended, L'\033', $u_extended_meta,NULL);
  $llp_Terminal_keymap = cons_lp($$lp_core_keybindings, $llp_Terminal_keymap);
}
