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
#include "execute.slc"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

/*
  TITLE: External Process Execution
  OVERVIEW: Provides an Activity representing an asynchronous execution of an
    external process.
 */

/*
  SYMBOL: $ls_process_executor_shell
    A list of strings which comprise the initial arguments used to execute a
    process. When executing a process, the command-line of the process will be
    added *as one argument* to the end of this list. The default is
      env(SHELL), -c
 */
STATIC_INIT_TO($ls_process_executor_shell,
               cons_s(getenv("SHELL") ?: "/bin/sh",
                      cons_s("-c", NULL)))

/*
  SYMBOL: $c_Executor
    Base class for process-execution activities. Note that the child process
    will not be executed until $m_execute() is called on the Executor.

  SYMBOL: $m_create_stdin_pipe $m_create_stdout_pipe $m_create_stderr_pipe
    Method on Executor. Sets up a pipe suitable for use as stdin/stdout/stderr
    on the child process, in $p_Executor_pipe[0] and $p_Executor_pipe[1]. The
    Executor class is oblivious to the meaning of these values, other than that
    they must be -1 or be valid file descriptors. These should indicate failure
    by rolling the transaction back.

  SYMBOL: $m_fixup_parent_stdin_pipe $m_fixup_parent_stdout_pipe
  SYMBOL: $m_fixup_parent_stderr_pipe
    Method on Executor. Called within the parent process after the Executor has
    forked. These methods should perform any "fixups" necessary on
    $p_Executor_pipe[0] and $p_Executor_pipe[1], usually in the form of closing
    one of the ends of the pipe (if it is a pipe) or closing the main
    descriptor (if it is just a file redirection). These SHOULD NOT roll the
    transaction back.

  SYMBOL: $m_fixup_child_stdin_pipe $m_fixup_child_stdout_pipe
  SYMBOL: $m_fixup_child_stderr_pipe
    Method on Executor. Called within the child process after the Executor has
    forked. These methods should perform any "fixups" necessary on
    $p_Executor_pipe[0] and $p_Executor_pipe[1] to set the child process's file
    descriptors appropriately. This includes using dup2() to relocate the
    descriptors to numbers 0, 1, and 2, respectively. Since these execute
    within the child process, they MUST NOT fork or do anything that would
    affect descriptors still shared with the parent process.

  SYMBOL: $p_Executor_pipe
    Type int (*)[2]. Parameter passed to the various pipe-setup methods,
    suitable for use to syscalls like pipe() and such.

  SYMBOL: $p_Executor_pipes
    Type int (*)[6]. An array of all possible file descriptors held by this
    Executor, to be closed automatically when it is destroyed.
 */
subclass($c_Activity, $c_Executor)

/*
  SYMBOL: $f_Executor_execute
    Spawns the child process run by this Executor. Calling this more than once
    on the same object has undefined results. If execution *setup* fails, the
    current transaction is rolled back. Execution failure can only be detected
    when the child dies. This function returns immediately after the child is
    forked(); it is entirely asynchronous.

  SYMBOL: $w_Executor_cmdline
    The command-line to pass to the subordinate process (ie, the shell).

  SYMBOL: $i_Executor_pid
    The pid of the child process being run by this Executor.

  SYMBOL: $f_Executor_fork
    Called within the child process of the Executor immediately after executing
    the fork() system call. This cannot be used as an abstract method; do not
    try to override it.
 */
defun($h_Executor_execute) {
  auto void cleanup_pipes(void);
  auto void kill_child(void);

  int* pipes = gcalloc(6*sizeof(int));
  memset(pipes, -1, sizeof(int)*6);
  tx_push_handler(cleanup_pipes);

  // Open the pipe pairs
  $M_create_stdin_pipe (0, 0, $p_Executor_pipe = pipes+0);
  $M_create_stdout_pipe(0, 0, $p_Executor_pipe = pipes+2);
  $M_create_stderr_pipe(0, 0, $p_Executor_pipe = pipes+4);

  // Set hook up
  add_hook_obj(&$h_sigchld, HOOK_MAIN,
               $u_Executor, $u_Executor,
               $m_sigchld, $o_Executor,
               NULL);

  // Pipes ready, spawn child
  pid_t child = fork();
  if (-1 == child)
    tx_rollback_errno($u_Executor);

  if (child) {
    // Parent process
    tx_push_handler(kill_child);
    $i_Executor_pid = child;
    $p_Executor_pipes = pipes;
    tx_write_through($i_Executor_pid);
    tx_write_through($p_Executor_pipes);

    $M_fixup_parent_stdin_pipe (0, 0, $p_Executor_pipe = pipes+0);
    $M_fixup_parent_stdout_pipe(0, 0, $p_Executor_pipe = pipes+2);
    $M_fixup_parent_stdout_pipe(0, 0, $p_Executor_pipe = pipes+4);
    tx_pop_handler();
  } else {
    // Child process
    $f_Executor_fork();
    $M_fixup_child_stdin_pipe (0, 0, $p_Executor_pipe = pipes+0);
    $M_fixup_child_stdout_pipe(0, 0, $p_Executor_pipe = pipes+2);
    $M_fixup_child_stderr_pipe(0, 0, $p_Executor_pipe = pipes+4);

    string cmdline = wstrtocstr($w_Executor_cmdline);
    string argv[llen_s($ls_process_executor_shell) + 2];
    unsigned ix = 0;
    each_s($ls_process_executor_shell,
           lambdav((string arg), argv[ix++] = arg));
    argv[ix++] = cmdline;
    argv[ix] = NULL;

    execv(argv[0], (char**)argv);

    // execv() failed
    // Our means of reporting failure are rather limitted here, since we have
    // no great way of talking to the parent, other than sending the error
    // message to stderr.
    string why = strerror(errno);
    write(2, why, strlen(why));
    _exit(-1);
  }

  tx_pop_handler();

  void cleanup_pipes(void) {
    for (unsigned i = 0; i < 6; ++i)
      if (pipes[i] != -1)
        close(pipes[i]);
  }

  void kill_child(void) {
    if (child > 0) {
      kill(child, SIGKILL);
      int ignored;
      waitpid(child, &ignored, 0);
    }
  }
}
