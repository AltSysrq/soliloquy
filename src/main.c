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

advise_after($h_Terminal) {
  fprintf(stderr, "Terminal status: %d\r\n",
          $y_Terminal_ok);
}

int main(void) {
  setlocale(LC_ALL, "");
  $o_term = $c_Terminal($p_Terminal_input = stdin,
                        $p_Terminal_output = stdout,
                        $s_Terminal_type = getenv("TERM"));

  if (!$($o_term, $y_Terminal_ok)) {
    $F_Terminal_destroy(0, $o_term, 0);
    fprintf(stderr, "Failed to initialise the terminal.\r\n");
    return 1;
  }

  within_context($o_term, {
      fprintf(stderr, "Colours: %d, Colour pairs: %d, %d rows x %d cols\r\n",
              $i_Terminal_num_colours, $i_Terminal_num_colour_pairs,
              $i_Terminal_rows, $i_Terminal_cols);
    });

  while (true) {
    within_context($o_term, {
        $f_Terminal_getch();
        if ($i_Terminal_input_type != -1)
          fprintf(stderr, "Input type = %d, input value = %d\r\n",
                  $i_Terminal_input_type, $i_Terminal_input_value);
        else {
          perror("getch");
        }
      });
  }
}
