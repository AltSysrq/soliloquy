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

class_keymap($c_Terminal, $$lp_main_keymap, $llp_Terminal_keymap)
class_keymap($c_Fizz_Buzz, $$lp_fb_keymap, $llp_Backing_keymap)

int main(void) {
  setlocale(LC_ALL, "");

  bind_char($$lp_main_keymap, $u_ground, CONTROL_C, NULL, $$f_quit);
  bind_char($$lp_fb_keymap, $u_ground, L'+', NULL, $f_Fizz_Buzz_go_faster);
  bind_char($$lp_fb_keymap, $u_ground, L'-', NULL, $f_Fizz_Buzz_go_slower);

  if (!($$o_term = $c_Terminal($s_Terminal_type = getenv("TERM"),
                               $p_Terminal_input = stdin,
                               $p_Terminal_output = stdout))) {
    perror("initialising terminal");
    return 1;
  }

  within_context($$o_term, ({
        $o_Terminal_current_view =
          $c_View($o_View_terminal = $$o_term,
                  $o_View_workspace =
                    $c_Workspace(
                      $o_Workspace_backing = $c_Fizz_Buzz()));}));

  $f_kernel_main();
  $M_destroy(0,$$o_term);
  return 0;
}

defun($$h_quit) {
  $y_keep_running = false;
}
