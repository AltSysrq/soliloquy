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

// The Executor constructor needs to augment these classes to
// register/deregister them from the Executor kernel object list.
member_of_domain($h_Consumer, $d_Executor)
member_of_domain($h_Producer, $d_Executor)
member_of_domain($h_Consumer_destroy, $d_Executor)
member_of_domain($h_Producer_destroy, $d_Executor)

defun($h_Executor) {
  // Add hooks to register/deregister Consumers and Producers owned by this
  // Executor
  add_hook(&$h_Consumer, HOOK_AFTER,
           $u_Executor, $u_registration,
           $m_register_consumer, NULL);
  add_hook(&$h_Producer, HOOK_AFTER,
           $u_Executor, $u_registration,
           $m_register_producer, NULL);
  add_hook(&$h_Consumer_destroy, HOOK_AFTER,
           $u_Executor, $u_registration,
           $m_deregister_consumer, NULL);
  add_hook(&$h_Producer_destroy, HOOK_AFTER,
           $u_Executor, $u_registration,
           $m_deregister_producer, NULL);
}

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

/*
  SYMBOL: $f_Executor_sigchld
    Hook function/method for the global $f_sigchld() hook point. Causes the
    Executor to check whether the child has died and, if it has, to collect
    exit information and take appropriate action.

  SYMBOL: $y_Executor_child_dead
    Set to true when the child process of this Executor has died.

  SYMBOL: $y_Executor_child_exited
    Set to true if the child process of this Executor exited of its own
    volition.

  SYMBOL: $i_Executor_child_return_value
    The 8-bit return value of the child process of this Executor. This value is
    only meaningful if the process has died. If it was killed by a signal, this
    is set to -1.

  SYMBOL: $y_Executor_child_killed
    Set to true of the child process of this Executor was killed by a signal.

  SYMBOL: $I_Executor_child_signal
    If the child process of this Executor was killed by a signal, set to the
    value of that signal.

  SYMBOL: $y_Executor_allow_hang
    If true, $f_Executor_sigchld() will block until the child process
    dies. This should only be used when it is known that the child will be dead
    very soon (ie, being SIGKILLed).
 */
defun($h_Executor_sigchld) {
  int status;
  if (0 < waitpid($i_Executor_pid, &status,
                  $y_Executor_allow_hang? 0 : WNOHANG)) {
    // The child has died
    $y_Executor_child_dead = true;
    $y_Executor_child_exited = !!WIFEXITED(status);
    if ($y_Executor_child_exited) {
      $i_Executor_child_return_value = WEXITSTATUS(status);
    } else {
      $i_Executor_child_return_value = -1;
    }

    $y_Executor_child_killed = !!WIFSIGNALED(status);
    if ($y_Executor_child_killed) {
      $I_Executor_child_signal = WTERMSIG(status);
    }

    $m_child_died();
  }
}

/*
  SYMBOL: $f_Executor_child_died
    To be called after the child process is dead. Cleans up all external
    resources, such as kernel objects and file handles. The Executor is then
    destroyed.
 */
defun($h_Executor_child_died) {
  // Remove the hook we added when execution began
  del_hook(&$h_sigchld, HOOK_MAIN, $u_Executor, $o_Executor);

  // Destroy all kernel objects
  on_each_o($lo_Executor_kernel_objects, $m_destroy);

  // Close all file handles
  int* pipes = $p_Executor_pipes;
  for (unsigned i = 0; i < 6; ++i)
    if (-1 != pipes[i])
      close(pipes[i]);

  if (!$y_Executor_is_being_destroyed)
    $m_destroy();
}

/*
  SYMBOL: $f_Executor_destroy
    If the child process of this Executor has been started and is not yet dead,
    it is killed and cleaned up. Then normal Activity destruction is
    continued.

  SYMBOL: $y_Executor_is_being_destroyed
    Set to true when $f_Executor_sigchld() is called from within
    $f_Executor_destroy(). This prevents calling $m_destroy() twice on the same
    object.
 */
defun($h_Executor_destroy) {
  if ($i_Executor_pid && !$y_Executor_child_dead) {
    kill($i_Executor_pid, SIGKILL);
    $y_Executor_allow_hang = true;
    $y_Executor_is_being_destroyed = true;
    $m_sigchld();
  }

  $f_Activity_destroy();
}

/*
  SYMBOL: $lo_Executor_kernel_objects
    A list of $m_destroy()able objects which are currently registered with the
    kernel and owned by this Executor, which must be deregistered when the
    Executor is destroyed or its child dies.

  SYMBOL: $f_Executor_register_producer $f_Executor_register_consumer
    Adds $o_Producer or $o_Consumer to $lo_Executor_kernel_objects.

  SYMBOL: $f_Executor_deregister_producer $f_Executor_deregister_consumer
    Removes $o_Producer or $o_Consumer from $lo_Executor_kernel_objects.
 */
defun($h_Executor_register_producer) {
  lpush_o($lo_Executor_kernel_objects, $o_Producer);
}

defun($h_Executor_register_consumer) {
  lpush_o($lo_Executor_kernel_objects, $o_Consumer);
}

defun($h_Executor_deregister_producer) {
  lpush_o($lo_Executor_kernel_objects, $o_Producer);
}

defun($h_Executor_deregister_consumer) {
  lpush_o($lo_Executor_kernel_objects, $o_Consumer);
}
