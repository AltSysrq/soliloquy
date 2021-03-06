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
#include "integration.slc"
#include "../interactive.h"
#include "../key_dispatch.h"

/*
  TITLE: Integration of process execution into the user interface.
 */

subclass($c_StdinFromLineEditor, $c_ExecuteAsyncProcess)
subclass($c_OutputToTranscript, $c_ExecuteAsyncProcess)

defun($h_ExecuteAsyncProcess_destroy) {
  $f_StdinFromLineEditor_destroy();
  $f_OutputToTranscript_destroy();
}

interactive($h_execute_async_process_i,
            $h_execute_async_process,
            i_(w, $w_Executor_cmdline, L"exec")) {
  object that = $c_ExecuteAsyncProcess(
    $y_OutputToTranscript_stdout = true,
    $y_OutputToTranscript_stderr = true,
    $w_Executor_prefix = L"!",
    $o_Activity_workspace = $o_Workspace);
  $M_execute(0, that);
}

class_keymap($c_BufferEditor, $lp_process_execution, $llp_Activity_keymap)
ATSINIT {
  bind_char($lp_process_execution, $u_ground, L'!', NULL,
            $f_execute_async_process_i);
}
