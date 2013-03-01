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
#include "transcript_tty.slc"

/*
  TITLE: Transcript-as-a-TTY Implementation
  OVERVIEW: Subclasses TtyEmulator to produce output to a Transcript, emulating
    a 1-row terminal.
 */

/*
  SYMBOL: $c_TranscriptTty
    Subclass of TtyEmulator. Emulates a one-row terminal; on each scroll event,
    the erased line becomes a permanent output line of the transcript. Output
    but not completed lines become temporary output lines on the transcript,
    and are updated as the line completes.
 */
subclass($c_TtyEmulator, $c_TranscriptTty)
member_of_domain($q_RenderedLine_meta, $d_TranscriptTty)

/*
  SYMBOL: $i_TranscriptTty_curr_line
    A Transcript mutable line reference. Equal to -1 if there is no current
    line. Otherwise, it indicates a mutable line in the transcript which holds
    the current, incomplete line of the output.
 */
STATIC_INIT_TO($i_TranscriptTty_curr_line, -1)

/*
  SYMBOL: $f_TranscriptTty_update
    Maintains the contents of the "current line" for this TranscriptTty.

  SYMBOL: $o_TranscriptTty_transcript
    The Transcript this TranscriptTty outputs to.

  SYMBOL: $y_TranscriptTty_update_force
    If true, $f_TranscriptTty_update() will produce a new output line even if
    that line will be blank.
 */
defun($h_TranscriptTty_update) {
  // Convert line contents to a qstring.
  mqstring line_contents = qcalloc($aax_TtyEmulator_screen->v[0]->len+1);
  qmemcpy(line_contents, $aax_TtyEmulator_screen->v[0]->v,
          $aax_TtyEmulator_screen->v[0]->len);
  // Change NULs before the last non-NUL to spaces.
  unsigned last_non_nul = 0;
  for (unsigned i = 0; i < $aax_TtyEmulator_screen->v[0]->len; ++i)
    if ($aax_TtyEmulator_screen->v[0]->v[i])
      last_non_nul = i;
  for (unsigned i = 0; i < last_non_nul; ++i)
    if (!($aax_TtyEmulator_screen->v[0]->v[i] & QC_CHAR))
      line_contents[i] |= L' ';

  // If we don't have a current line, see whether it is worth creating one
  if (-1 == $i_TranscriptTty_curr_line) {
    bool has_any = false;
    for (unsigned i = 0; i < $aax_TtyEmulator_screen->v[0]->len; ++i) {
      if ($aax_TtyEmulator_screen->v[0]->v[i]) {
        has_any = true;
        break;
      }
    }

    if (!has_any && !$y_TranscriptTty_update_force)
      // Nothing to display in the current line, don't create it yet
      return;

    // OK, create the new line
    $i_TranscriptTty_curr_line =
      $M_add_ref_line($i_Transcript_line_ref, $o_TranscriptTty_transcript,
                      $o_Transcript_ref_line =
                        $c_RenderedLine(
                          $q_RenderedLine_body = line_contents));
  } else {
    // Update the line
    $M_change_ref_line(0, $o_TranscriptTty_transcript,
                       $i_Transcript_line_ref = $i_TranscriptTty_curr_line,
                       $o_Transcript_ref_line =
                         $c_RenderedLine(
                           $q_RenderedLine_body = line_contents));
  }

  // Call super
  $f_TtyEmulator_update();
}

/*
  SYMBOL: $f_TranscriptTty_scroll
    Ensures that the current line has been displayed, then releases it so that
    a new line can be produced. Then calls $f_TtyEmulator_scroll().
 */
defun($h_TranscriptTty_scroll) {
  let($y_TranscriptTty_update_force, true);
  $m_update();
  $M_release_ref_line(0, $o_TranscriptTty_transcript,
                      $i_Transcript_line_ref = $i_TranscriptTty_curr_line);
  $i_TranscriptTty_curr_line = -1;

  // Call super
  $f_TtyEmulator_scroll();
}

/*
  SYMBOL: $f_TranscriptTty_destroy
    Releases any mutable line reference held by this TranscriptTty, then calls
    $f_TtyEmulator_destroy().
 */
defun($h_TranscriptTty_destroy) {
  // Release the mutable line if we are still holding it
  if (-1 != $i_TranscriptTty_curr_line)
    $M_release_ref_line(0, $o_TranscriptTty_transcript,
                        $i_Transcript_line_ref = $i_TranscriptTty_curr_line);

  // Call super
  $f_TtyEmulator_destroy();
}
