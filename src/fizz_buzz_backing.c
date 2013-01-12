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
#include "fizz_buzz_backing.slc"

/*
  TITLE: "Fuzz Buzz" Test Backing
  OVERVIEW: A test backing for debugging which periodically appends and/or
    replaces text.
*/

subclass($c_Backing,$c_Fizz_Buzz)

defun($h_Fizz_Buzz) {
  add_hook_obj(&$h_run_tasks, HOOK_MAIN,
               $u_Fizz_Buzz, $u_Fizz_Buzz,
               $f_Fizz_Buzz_task, $o_Fizz_Buzz,
               NULL);
}

defun($h_Fizz_Buzz_task) {
  $y_kernel_poll_infinite = false;
  if ($i_kernel_poll_duration_ms > 256)
    $i_kernel_poll_duration_ms = 256;

  wchar_t str[32];
  int ix = ++$i_Fizz_Buzz_ix;

  if (ix % 5 == 0 && ix % 3 == 0) {
    $F_Backing_alter(0,0,
                     $i_Backing_alteration_begin =
                       $i_Fizz_Buzz_fbix++,
                     $i_Backing_ndeletions = 1,
                     $lo_Backing_replacements =
                       cons_o(
                         $c_Rendered_Line(
                           $q_Rendered_Line_meta = qempty,
                           $q_Rendered_Line_body =
                             wstrtoqstr(L"fizzbuzz")),
                         NULL));
  } else {
    wstring wstr;
    if (ix % 3 == 0)
      wstr = L"fizz";
    else if (ix % 5 == 0)
      wstr = L"buzz";
    else {
      swprintf(str, sizeof(str), L"%d", ix);
      wstr = str;
    }

    $F_Backing_alter(0,0,
                     $i_Backing_alteration_begin =
                       $ao_Backing_lines->len,
                     $i_Backing_ndeletions = 0,
                     $lo_Backing_replacements =
                       cons_o(
                         $c_Rendered_Line(
                           $q_Rendered_Line_meta = qempty,
                           $q_Rendered_Line_body = wstrtoqstr(wstr)),
                         NULL));
  }
}
