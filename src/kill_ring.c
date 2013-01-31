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
#include "kill_ring.slc"

/*
  TITLE: Kill Ring Management
  OVERVIEW: Provides functions to manage the Emacs-style kill ring. Note that
    there are two separate kill rings: one for character-level editing, and one
    for line-level.
 */

/*
  SYMBOL: $aw_c_kill_ring $aaw_l_kill_ring
    The character-oriented and line-oriented kill rings, respectively. The most
    recently-killed item is recorded in $I_c_kill_ring and $I_l_kill_ring,
    respectively. Note that both may have NULL entries at arbitrary locations.

  SYMBOL: $I_c_kill_ring $I_l_kill_ring
    The indices into $aw_c_kill_ring and $aaw_l_kill_ring, respectively, of the
    most recently killed item.
 */

ATSINIT {
  $aw_c_kill_ring = dynar_new_w();
  dynar_expand_by_w($aw_c_kill_ring, 16);
  $aaw_l_kill_ring = dynar_new_aw();
  dynar_expand_by_aw($aaw_l_kill_ring, 8);
}

/*
  SYMBOL: $f_c_kill
    Kills a string of text, modifying the character-oriented kill ring
    appropriately. This should only be called once per logical command;
    otherwise, the results will be rather unintuitive. The string to kill is
    stored in $w_kill. $v_kill_direction must also be set appropriately.

  SYMBOL: $w_kill
    The string to kill in a call to $f_c_kill.

  SYMBOL: $v_kill_direction
    In a call to $f_c_kill() or $f_l_kill(), the direction relative to the
    cursor of the killed text. This should be either $u_forward or
    $u_backward.

  SYMBOL: $y_LastCommand_was_c_kill
    When true in the context of $o_prev_command, $f_c_kill() will concatenate
    the newly-killed text instead of creating a new entry.

  SYMBOL: $u_forward $u_backward
    In kill functions, identify the direction of the killed text relative to
    the cursor.
 */
defun($h_c_kill) {
  wstring base;
  if ($($o_prev_command, $y_LastCommand_was_c_kill)) {
    base = $aw_c_kill_ring->v[$I_c_kill_ring];
  } else {
    base = L"";
    $I_c_kill_ring = ($I_c_kill_ring + 1) % $aw_c_kill_ring->len;
  }

  wstring new;
  if ($v_kill_direction == $u_forward)
    new = wstrap(base, $w_kill);
  else
    new = wstrap($w_kill, base);

  $aw_c_kill_ring->v[$I_c_kill_ring] = new;
  $$($o_this_command) {
    $y_LastCommand_was_c_kill = true;
  }
}

/*
  SYMBOL: $f_l_kill
    Kills a series of lines of text, modifying the line-oriented kill ring
    appropriately. This should only be called once per logical command;
    otherwise, the results will be rather unintuitive. The lines to kill are
    stored in $lw_kill. $v_kill_direction must also be set appropriately.

  SYMBOL: $lw_kill
    The lines to kill in a call to $f_l_kill().

  SYMBOL: $y_LastCommand_was_l_kill
    When true in the context of $o_prev_command, $f_l_kill() will concatenate
    the newly-killed text instead of creating a new entry.
 */
defun($h_l_kill) {
  dynar_w base;
  if ($($o_prev_command, $y_LastCommand_was_l_kill)) {
    base = $aaw_l_kill_ring->v[$I_l_kill_ring];
  } else {
    $I_l_kill_ring = ($I_l_kill_ring + 1) % $aaw_l_kill_ring->len;
    base = $aaw_l_kill_ring->v[$I_l_kill_ring] = dynar_new_w();
  }

  unsigned len = llen_w($lw_kill);
  wstring data[len];
  unsigned ix = 0;
  each_w($lw_kill, lambdav((wstring w), data[ix++] = w));

  if ($v_kill_direction == $u_backward)
    dynar_ins_w(base, 0, data, len);
  else
    dynar_ins_w(base, base->len, data, len);

  $$($o_this_command) {
    $y_LastCommand_was_l_kill = true;
  }
}
