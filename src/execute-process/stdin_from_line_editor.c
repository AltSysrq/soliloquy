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
#include "stdin_from_line_editor.slc"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/*
  TITLE: Process stdin from line editor
  OVERVIEW: Allows to provide input to a process via a LineEditor.
 */

/*
  SYMBOL: $c_StdinFromLineEditor
    Subclass of LineEditor and Executor which provides input to the process by
    allowing the user to provide it by entering it a line at a time via line
    editing.
 */
subclass($c_LineEditor, $c_StdinFromLineEditor)
subclass($c_Executor, $c_StdinFromLineEditor)
subclass($c_Producer, $c_SfleProducer)

/*
  SYMBOL: $as_StdinFromLineEditor_buffer
    The input queue provided by the user so far.
 */
defun($h_StdinFromLineEditor) {
  $as_StdinFromLineEditor_buffer = dynar_new_s();
}

/*
  SYMBOL: $f_StdinFromLineEditor_create_stdin_pipe
    Creates the pipe and performs initial setup.
 */
defun($h_StdinFromLineEditor_create_stdin_pipe) {
  int* pipes = $p_Executor_pipe;
  if (-1 == pipe(pipes) ||
      -1 == fcntl(pipes[1], F_SETFL, O_NONBLOCK))
    tx_rollback_errno($u_StdinFromLineEditor);
}

/*
  SYMBOL: $f_StdinFromLineEditor_fixup_parent_stdin_pipe
    Closes the child's end of the pipe and begins waiting for the pipe to
    become available.
 */
defun($h_StdinFromLineEditor_fixup_parent_stdin_pipe) {
  int* pipes = $p_Executor_pipe;
  // Close read end of the pipe
  close(pipes[0]);
  pipes[0] = -1;

  // Save the write file descriptor
  implant($i_Producer_fd);
  $i_Producer_fd = pipes[1];

  $m_begin_waiting();
}

/*
  SYMBOL: $f_StdinFromLineEditor_fixup_child_stdin_pipe
    Closes the parent's end of the pipe and moves the child's to stdin.
 */
defun($h_StdinFromLineEditor_fixup_child_stdin_pipe) {
  int* pipes = $p_Executor_pipe;
  // Close the write end of the pipe
  close(pipes[1]);
  // Move the read end to stdin
  dup2(pipes[0], STDIN_FILENO);
  close(pipes[0]);
}

/*
  SYMBOL: $f_StdinFromLineEditor_begin_waiting
    Creates $o_StdinFromLineEditor_producer if it is NULL.

  SYMBOL: $o_StdinFromLineEditor_producer
    If non-NULL, a Producer waiting for the write side of the stdin pipe to
    become ready. If NULL, the pipe is currently ready.
 */
defun($h_StdinFromLineEditor_begin_waiting) {
  if (!$o_StdinFromLineEditor_producer) {
    $o_StdinFromLineEditor_producer = $c_SfleProducer();
    $M_update_echo_area(0, $o_Activity_workspace);
  }
}

/*
  SYMBOL: $f_StdinFromLineEditor_pump_input
    Pushes as much data as possible to the output pipe. If writing to the pipe
    would block, $m_begin_waiting() is called.
 */
defun($h_StdinFromLineEditor_pump_input) {
  /* If there was a producer, destroy it since we now have input */
  if ($o_StdinFromLineEditor_producer) {
    $M_destroy(0, $o_StdinFromLineEditor_producer);
    $M_update_echo_area(0, $o_Activity_workspace);
  }

  while ($as_StdinFromLineEditor_buffer->len) {
    int written = write($i_Producer_fd, $as_StdinFromLineEditor_buffer->v[0],
                        strlen($as_StdinFromLineEditor_buffer->v[0]));
    if (-1 == written) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        //Too much input, must wait for the pipe to become available again
        $m_begin_waiting();
      } else {
        // Other error, take no further action
      }
      return;
    }

    // Advance the buffer
    $as_StdinFromLineEditor_buffer->v[0] += written;
    if (!*$as_StdinFromLineEditor_buffer->v[0])
      dynar_erase_s($as_StdinFromLineEditor_buffer, 0, 1);
  }
}

/*
  SYMBOL: $f_StdinFromLineEditor_accept
    Queues the entered line, then calls $m_pump_input(). The LineEditor is then
    reset.
 */
defun($h_StdinFromLineEditor_accept) {
  // Push this line of input through
  dynar_push_s($as_StdinFromLineEditor_buffer,
               wstrtocstr($w_LineEditor_text));
  dynar_push_s($as_StdinFromLineEditor_buffer, "\n");
  if (!$o_StdinFromLineEditor_producer)
    $m_pump_input();

  // Reset the line editor
  $m_push_undo();
  $az_LineEditor_buffer = dynar_new_z();
  $i_LineEditor_point = 0;

  $M_update_echo_area(0, $o_Activity_workspace);
}

/*
  SYMBOL: $c_SfleProducer
    Producer subclass which calls $m_pump_input() on its associated
    StdinFromLineEditor when ready. When constructed, it should inherit
    $i_Producer_fd from the calling context.

  SYMBOL: $o_SfleProducer_slfe
    The StdinFromLineEditor associated with this SfleProducer.
 */
defun($h_SfleProducer) {
  $o_SfleProducer_slfe = $o_StdinFromLineEditor;
}

/*
  SYMBOL: $f_SfleProducer_write
    Calls $m_pump_input() on $o_SfleProducer_slfe.
 */
defun($h_SfleProducer_write) {
  $M_pump_input(0, $o_SfleProducer_slfe);
}
