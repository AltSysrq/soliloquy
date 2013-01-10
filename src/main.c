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

#ifdef DEBUG
ATSTART(wait_for_debugging, 101) {
  fprintf(stderr, "Press ENTER to continue...\n");
  getc(stdin);
}
#endif

int main(void) {
  setlocale(LC_ALL, "");

  if (!($$o_term = $c_Terminal($s_Terminal_type = getenv("TERM"),
                               $p_Terminal_input = stdin,
                               $p_Terminal_output = stdout))) {
    perror("initialising terminal");
    return 1;
  }

  $f_kernel_main();
  return 0;
}

advise_after($h_Terminal_getch) {
  qchar qc = $i_Terminal_input_value;
  let($q_qch, &qc);
  $F_Terminal_putch(0,0, ++$i_x);
}
