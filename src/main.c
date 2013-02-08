/*
  Copyright ⓒ 2012, 2013 Jason Lingle

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

/*
  TITLE: Program Entry-Point
  OVERVIEW: Performs basic initialisation.
*/

#include "main.slc"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include "key_dispatch.h"

#ifdef DEBUG
ATSTART(wait_for_debugging, 101) {
  fprintf(stderr, "Press ENTER to continue...\n");
  getc(stdin);
}
#endif

ATSTART(initialise_gc, 102) {
  GC_init();
}

class_keymap($c_Terminal, $$lp_main_keymap, $llp_Terminal_keymap)
class_keymap($c_Workspace, $$lp_main_keymap2, $llp_Workspace_keymap)

int main(void) {
  setlocale(LC_ALL, "");

  bind_char($$lp_main_keymap, $u_extended, CONTROL_C, NULL, $$f_quit);
  bind_char($$lp_main_keymap2, $u_extended, CONTROL_X, $u_ground, $$f_die);

  if (!($$o_term = $c_Terminal($s_Terminal_type = getenv("TERM"),
                               $p_Terminal_input = stdin,
                               $p_Terminal_output = stdout))) {
    perror("initialising terminal");
    return 1;
  }

  $$($$o_term) {
    object workspace =
      $c_Workspace(
        $o_Workspace_backing =
          $c_Transcript());
    $$(workspace) {
      $o_Terminal_current_view =
        $c_View($o_View_terminal = $$o_term,
                $o_View_workspace = workspace);
      $c_FileBuffer(
        $w_FileBuffer_filename = L"*scratch*",
        $y_FileBuffer_memory_backed = true);
      $c_TopLevel();
    }
    $M_redraw(0,$o_Terminal_current_view);
  }

  $f_kernel_main();
  $M_destroy(0,$$o_term);
  return 0;
}

defun($$h_quit) {
  $y_keep_running = false;
}

defun($$h_die) {
  static char reason[64];
  $v_rollback_type = $$u_user_triggered;
  sprintf(reason, "User triggered: %d",
          $$i_die_ix++);
  $s_rollback_reason = reason;
  tx_rollback();
}
