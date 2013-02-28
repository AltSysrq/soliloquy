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
#include "output_to_transcript.slc"
#include <unistd.h>
#include <fcntl.h>

/*
  TITLE: Process Output to Transcript
  OVERVIEW: Provides facilities to direct process output (stdout &/| stderr) to
    the Transcript.
 */

/*
  SYMBOL: $c_OutputToTranscript
    Provides facilities to direct process output to the Transcript.

  SYMBOL: $y_OutputToTranscript_stdout $y_OutputToTranscript_stderr
    If true, the $c_OutputToTranscript constructor will set the
    H_*_stdout_pipe or H_*_stderr_pipe abstract methods to the
    implementations provided by this class. At least one of these should be
    true for this class to have any effect.
 */
subclass($c_Executor, $c_OutputToTranscript)
defun($h_OutputToTranscript) {
  if ($y_OutputToTranscript_stdout) {
    $H_create_stdout_pipe = &$h_OutputToTranscript_create_pipe;
    $H_fixup_parent_stdout_pipe = &$h_OutputToTranscript_fixup_parent_pipe;
    $H_fixup_child_stdout_pipe = &$h_OutputToTranscript_fixup_child_pipe;
  }
  if ($y_OutputToTranscript_stderr) {
    $H_create_stderr_pipe = &$h_OutputToTranscript_create_pipe;
    $H_fixup_parent_stderr_pipe = &$h_OutputToTranscript_fixup_parent_pipe;
    $H_fixup_child_stderr_pipe = &$h_OutputToTranscript_fixup_child_pipe;
  }
}

/*
  SYMBOL: $f_OutputToTranscript_create_pipe
    Potential implementation of m_create_std{out,err}_pipe.
 */
defun($h_OutputToTranscript_create_pipe) {
  int* pipes = $p_Executor_pipe;
  if (-1 == pipe(pipes) ||
      -1 == fcntl(pipes[0], F_SETFL, O_NONBLOCK))
    tx_rollback_errno($u_OutputToTranscript);
}

/*
  SYMBOL: $f_OutputToTranscript_fixup_parent_pipe
    Potential implementation of m_fixup_parent_std{out,err}_pipe.
 */
defun($h_OutputToTranscript_fixup_parent_pipe) {
  int* pipes = $p_Executor_pipe;
  // Close the write end of the pipe (which belongs to the child)
  close(pipes[1]);
  pipes[1] = -1;

  // Create emulator on our end
  $c_TtyConsumer(
    $o_TtyConsumer_emulator =
    $c_TranscriptTty($i_Consumer_fd = pipes[0],
                     $o_TranscriptTty_transcript = $o_Transcript));
}

/*
  SYMBOL: $f_OutputToTranscript_fixup_child_pipe
    Potential implementation of m_fixup_child_std{out,err}_pipe.
 */
defun($h_OutputToTranscript_fixup_child_pipe) {
  int* pipes = $p_Executor_pipe;
  // Close the read (parent) end of the pipe
  close(pipes[0]);
  pipes[0] = -1;

  // Move the fd to the appropriate number
  dup2(pipes[1], $i_Executor_target_fd);
  close(pipes[1]);
}
