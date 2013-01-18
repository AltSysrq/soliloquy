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
#include "primes_transcript.slc"

/*
  TITLE: Prime Number Calculator
  OVERVIEW: Subclasses Transcript with a prime number calculator to test
    Transcript's functionality.
*/

subclass($c_Transcript, $c_Primes_Transcript)
STATIC_INIT_TO($i_Primes_Transcript_delay, 1024)

defun($h_Primes_Transcript) {
  $i_Primes_Transcript_val = 2;
  add_hook_obj(&$h_run_tasks, HOOK_MAIN,
               $u_Primes_Transcript, $u_Primes_Transcript,
               $f_Primes_Transcript_task, $o_Primes_Transcript,
               NULL);
}

advise_after($h_Primes_Transcript) {
  $i_Primes_Transcript_headline =
    $M_add_ref_line($i_Transcript_line_ref,0,
                    $o_Transcript_ref_line =
                      $c_Rendered_Line(
                        $q_Rendered_Line_body = qempty,
                        $q_Rendered_Line_meta = qempty));
}

defun($h_Primes_Transcript_task) {
  $y_kernel_poll_infinite = false;
  if ($i_kernel_poll_duration_ms > $i_Primes_Transcript_delay)
    $i_kernel_poll_duration_ms = $i_Primes_Transcript_delay;

  wchar_t str[32];

  //First, update the head line
  swprintf(str, sizeof(str), L"Current: %d", $i_Primes_Transcript_val);
  $M_change_ref_line(0,0,
                     $i_Transcript_line_ref = $i_Primes_Transcript_headline,
                     $o_Transcript_ref_line =
                       $c_Rendered_Line(
                         $q_Rendered_Line_meta = qempty,
                         $q_Rendered_Line_body = wstrtoqstr(str)));

  // Is it prime?
  bool prime = true;
  for (int i = 2; i < $i_Primes_Transcript_val && prime; ++i)
    prime &= !!($i_Primes_Transcript_val % i);

  if (!prime) {
    //Add to the current list of non-primes
    swprintf(str, sizeof(str), L"%d", $i_Primes_Transcript_val);
    $lo_Primes_Transcript_np = cons_o(
      $c_Rendered_Line($q_Rendered_Line_body = wstrtoqstr(str),
                       $q_Rendered_Line_meta = wstrtoqstr(L"---")),
      $lo_Primes_Transcript_np);
  } else {
    // Output current non-primes, reset accum, then notify that this is
    // non-prime
    $M_group(0,0, $lo_Transcript_output = $lo_Primes_Transcript_np);
    $lo_Primes_Transcript_np = NULL;

    swprintf(str, sizeof(str), L"Prime: %d!", $i_Primes_Transcript_val);
    $M_append(0,0,
              $lo_Transcript_output =
                cons_o(
                  $c_Rendered_Line(
                    $q_Rendered_Line_body = wstrtoqstr(str),
                    $q_Rendered_Line_meta = wstrtoqstr(L"p!")),
                  NULL));
  }

  ++$i_Primes_Transcript_val;
}
