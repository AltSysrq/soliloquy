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

subclass($c_Transcript, $c_PrimesTranscript)
STATIC_INIT_TO($i_PrimesTranscript_delay, 1024)

defun($h_PrimesTranscript) {
  $i_PrimesTranscript_val = 2;
  add_hook_obj(&$h_run_tasks, HOOK_MAIN,
               $u_PrimesTranscript, $u_PrimesTranscript,
               $f_PrimesTranscript_task, $o_PrimesTranscript,
               NULL);
}

advise_after($h_PrimesTranscript) {
  $i_PrimesTranscript_headline =
    $M_add_ref_line($i_Transcript_line_ref,0,
                    $o_Transcript_ref_line =
                      $c_RenderedLine(
                        $q_RenderedLine_body = qempty,
                        $q_RenderedLine_meta = qempty));
}

defun($h_PrimesTranscript_task) {
  $y_kernel_poll_infinite = false;
  if ($i_kernel_poll_duration_ms > $i_PrimesTranscript_delay)
    $i_kernel_poll_duration_ms = $i_PrimesTranscript_delay;

  wchar_t str[32];

  //First, update the head line
  swprintf(str, sizeof(str), L"Current: %d", $i_PrimesTranscript_val);
  $M_change_ref_line(0,0,
                     $i_Transcript_line_ref = $i_PrimesTranscript_headline,
                     $o_Transcript_ref_line =
                       $c_RenderedLine(
                         $q_RenderedLine_meta = qempty,
                         $q_RenderedLine_body = wstrtoqstr(str)));

  // Is it prime?
  bool prime = true;
  for (int i = 2; i < $i_PrimesTranscript_val && prime; ++i)
    prime &= !!($i_PrimesTranscript_val % i);

  if (!prime) {
    //Add to the current list of non-primes
    swprintf(str, sizeof(str), L"%d", $i_PrimesTranscript_val);
    $lo_PrimesTranscript_np = cons_o(
      $c_RenderedLine($q_RenderedLine_body = wstrtoqstr(str),
                       $q_RenderedLine_meta = wstrtoqstr(L"XXX")),
      $lo_PrimesTranscript_np);
  } else {
    // Output current non-primes, reset accum, then notify that this is
    // non-prime
    $M_group(0,0, $lo_Transcript_output = $lo_PrimesTranscript_np);
    $lo_PrimesTranscript_np = NULL;

    swprintf(str, sizeof(str), L"Prime: %d!", $i_PrimesTranscript_val);
    $M_append(0,0,
              $lo_Transcript_output =
                cons_o(
                  $c_RenderedLine(
                    $q_RenderedLine_body = wstrtoqstr(str),
                    $q_RenderedLine_meta = wstrtoqstr(L"p!")),
                  NULL));
  }

  ++$i_PrimesTranscript_val;
}
