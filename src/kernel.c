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

/*
  TITLE: Input/Output Multiplexing and Task Control

  OVERVIEW: The kernel module dispatches input, output, and timer events to
    interested parties, in order to facilitate cooperative multi-tasking.
*/

#include "kernel.slc"

#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <poll.h>
#include <errno.h>

static void handle_sigchld(int, siginfo_t*, void*);
static void handle_quit(int);
static void handle_fatal(int);

ATSTART(initialise_kernel, STATIC_INITIALISATION_PRIORITY) {
  /* The following signals are interesting to us and need to be handled
   * synchronously. Block them except when polling for input (ie, so we'll
   * always be in a consistent state).
   *
   * SIGCHLD --- Inferior process terminated / signalled
   * SIGTERM, SIGHUP --- Graceful but immediate termination
   *
   * We don't add SIGQUIT so there is still a way to end Soliloquy
   * semi-gracefully if it enters an infinite loop.
   */
  sigset_t to_block;
  sigemptyset(&to_block);
  sigaddset(&to_block, SIGCHLD);
  sigaddset(&to_block, SIGTERM);
  sigaddset(&to_block, SIGHUP);
  sigprocmask(SIG_BLOCK, &to_block, NULL);

  struct sigaction action;
  memset(&action, 0, sizeof(action));

  action.sa_mask = to_block;
  action.sa_sigaction = handle_sigchld;
  action.sa_flags = SA_SIGINFO;
  sigaction(SIGCHLD, &action, NULL);

  memset(&action, 0, sizeof(action));
  action.sa_mask = to_block;
  action.sa_handler = handle_quit;
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGHUP , &action, NULL);
  sigaction(SIGQUIT, &action, NULL);
  action.sa_handler = handle_fatal;
  sigaction(SIGSEGV, &action, NULL);
  sigaction(SIGILL , &action, NULL);

  //Nothing to do on SIGPIPE
  signal(SIGPIPE, SIG_IGN);
}

/*
  SYMBOL: $i_async_signal
    When an asynchronous signal is being handled, this is set to the number of
    that signal. Its value is undefined otherwise.

  SYMBOL: $y_signal_is_synchronous
    Set to true iff an asynchronous signal is being handled AND it is being
    done so synchronously. If this is true, all functions are safe to call, as
    the program is in a known-consistent state. If it is false, but
    $y_is_handling_signal is true, there is very little you can do. In
    particular, any action that could possibly interact with the heap is
    off-limits and has undefined behaviour.

  SYMBOL: $y_is_handling_signal
    Set to true iff an asynchronous signal is currently being handled (even if
    it is being handled synchronously).

  SYMBOL: $i_sigchld_code $i_sigchld_pid $i_sigchld_status
    When a SIGCHLD signal is being handled, set to the values of
    siginfo_t::si_code, ::si_pid, and ::si_status for that signal (see
    sigaction(2)).

  SYMBOL: $f_sigchld
    Called to handle incomming SIGCHLD signals. These are always handled
    synchronously.
*/

static void handle_sigchld(int sigchld, siginfo_t* info, void* unknown) {
  $i_async_signal = sigchld;
  $y_signal_is_synchronous = true;
  $i_sigchld_code = info->si_code;
  $i_sigchld_pid = info->si_pid;
  $i_sigchld_status = info->si_status;

  $y_is_handling_signal = true;
  $f_sigchld();
  $y_is_handling_signal = false;
}

/*
  SYMBOL: $f_save_the_world
    Called when the program must be shut down abruptly. Hooks should be added
    to save unmodified changes to recoverable locations; *nothing* should be
    overwritten. *This may be called during an asynchronous signal handler.*
    Hooks should try not to call non-async-safe operations, though some laxness
    is tolerated here since this generally represents an already panicked
    state. Hooks should also try to avoid assuming too much about the program
    state, since this may be called as a result of a segmentation fault or
    abort.

  SYMBOL: $f_graceful_exit
    Called after $f_save_the_world when the program is being terminated due to
    an external signal (such as SIGTERM) rather than internal problems. It will
    never be run in an asynchronous context.

  SYMBOL: $f_die_gracelessly
    Called after $f_save_the_world when the program is terminating due to
    internal problems (such as receiving SIGSEGV). Hooks should try to clean up
    _external_ concerns as best they can. Note that this is also called in the
    case of SIGQUIT, since that signal is asynchronous. (The idea here is that
    the user will only send SIGQUIT if SIGTERM fails to terminate the program,
    which indicates an internal problem anyway, eg, an infinite loop.)
*/

static void handle_quit(int which) {
  $i_async_signal = which;
  $y_signal_is_synchronous = (which != SIGQUIT);
  $y_is_handling_signal = true;

  $f_save_the_world();
  if ($y_signal_is_synchronous)
    $f_graceful_exit();
  else
    $f_die_gracelessly();
  exit(0);
}

static void handle_fatal(int which) {
  $i_async_signal = which;
  $y_signal_is_synchronous = true;
  $y_is_handling_signal = true;

  $f_save_the_world();
  $f_die_gracelessly();

  // Die with the same signal, the way the OS would like
  signal(which, SIG_DFL);
  raise(which);
}


/*
  SYMBOL: $c_Consumer
    Encapsulates the state for an object which consumes data from an input file
    descriptor. When read(2) will not block, the $m_read method will be called
    on the Consumer; this method must not block.

  SYMBOL: $i_Consumer_fd
    The file descriptor backing the Consumer. It *must* be valid.

  SYMBOL: $y_Consumer_has_priority
    Within the $m_read method of a Consumer, indicates whether "priority" data
    is available.

  SYMBOL: $f_Consumer_destroy
    Removes the Consumer from the consumer list.
 */

defun($h_Consumer) {
  $$lo_consumers = cons_o($o_Consumer, $$lo_consumers);
}

defun($h_Consumer_destroy) {
  $$lo_consumers = lrm_o($$lo_consumers, $o_Consumer);
}

/*
  SYMBOL: $c_Producer
    Encapsulates the state for an object which produces data to an output file
    descriptor. When write(2) will not block, the $m_write method will be
    called on the Producer; this method must not block.

  SYMBOL: $i_Producer_fd
    The file descriptor backing the Producer. It *must* be valid.

  SYMBOL: $y_Producer_ready $y_Producer_hungup $y_Producer_error
    When the $m_write method is called, these indicate the states of POLLOUT,
    POLLHUP, and POLLERR for the Producer's file descriptor. (See poll(2).)

  SYMBOL: $f_Producer_destroy
    Removes the Producer from the producer list.
 */

defun($h_Producer) {
  $$lo_producers = cons_o($o_Producer, $$lo_producers);
}

defun($h_Producer_destroy) {
  $$lo_producers = lrm_o($$lo_producers, $o_Producer);
}

/*
  SYMBOL: $y_keep_running
    When set to false, the kernel exits when the current cycle completes.
*/
advise_before($h_kernel_main) {
  $y_keep_running = true;
}

/*
  SYMBOL: $f_kernel_main
    The main loop of the kernel. Typically called once for the whole program.
*/
defun($h_kernel_main) {
  while ($y_keep_running)
    $f_kernel_cycle();
}

/*
  SYMBOL: $f_kernel_cycle
    Performs one task/io cycle; called by $h_kernel_main. Typically, you want
    to hook onto $h_run_tasks() instead.

  SYMBOL: $f_run_tasks
    Called once per kernel cycle. Any periodic or cooperative tasks that need
    to run should hook themselves into this point. When this is invoked,
    $i_kernel_poll_duration_ms is 2**31-1 and $y_kernel_poll_infinite is
    true. Hooks may alter these if they need to run on a temporal
    basis. Conventionally, no hook should reset $y_kernel_poll_infinite to true
    or increase $i_kernel_poll_duration_ms.

  SYMBOL: $i_kernel_poll_duration_ms $y_kernel_poll_infinite
    These two control the maximum length of the kernel cycle. If
    $y_kernel_poll_infinite is true after the completion of $f_run_tasks, the
    current kernel cycle will last until the next I/O event. If it is false,
    the next kernel cycle will begin in approximately
    $i_kernel_poll_duration_ms milliseconds or after the next I/O event,
    whichever comes first.
 */
defun($h_kernel_cycle) {
  // Reset poll duration variables, then run one cycle for all tasks. If any
  // need to altern the poll duration, they can do so.
  $i_kernel_poll_duration_ms = 0x7FFFFFFF;
  $y_kernel_poll_infinite = true;
  $f_run_tasks();

  if (GC_collect_a_little()) {
    $y_kernel_poll_infinite = false;
    $i_kernel_poll_duration_ms = 0;
  }

  unsigned fdcount = llen_o($$lo_producers) + llen_o($$lo_consumers);
  {
    struct pollfd fds[fdcount];
    unsigned ix = 0;
    void inputfd(object obj) {
      fds[ix].fd = $(obj, $i_Consumer_fd);
      fds[ix].events = POLLIN | POLLPRI;
      ++ix;
    }
    each_o($$lo_consumers, inputfd);

    void outputfd(object obj) {
      fds[ix].fd = $(obj, $i_Producer_fd);
      fds[ix].events = POLLOUT;
    }
    each_o($$lo_producers, outputfd);

    sigset_t allow_all;
    sigemptyset(&allow_all);
#ifdef HAVE_PPOLL
    struct timespec timeout = {
      .tv_sec = $i_kernel_poll_duration_ms / 1000,
      .tv_nsec = ($i_kernel_poll_duration_ms % 1000) * 1000000L,
    };

    int ret = ppoll(fds, fdcount,
                    $y_kernel_poll_infinite? NULL : &timeout,
                    &allow_all);
#else
    // Race condition, but at worst will stall things until the next keystroke
    sigset_t oldsigset;
    sigprocmask(SIG_SETMASK, &allow_all, &oldsigset);
    int ret = poll(fds, fdcount,
                   $y_kernel_poll_infinite? -1 : $i_kernel_poll_duration_ms);
    sigprocmask(SIG_SETMASK, &oldsigset, NULL);
#endif

    if (ret == -1) {
      //Error
      if (errno != EINTR)
        perror("poll");
    } else if (ret) {
      //One or more input sources ready
      ix = 0;
      void pr_input(object obj) {
        if (fds[ix].revents)
          $M_read(0, obj,
                  $y_Consumer_has_priority =
                      (fds[ix].revents & POLLPRI));
        ++ix;
      }
      each_o($$lo_consumers, pr_input);

      void pr_output(object obj) {
        if (fds[ix].revents)
          $M_write(0, obj,
                   $y_Producer_ready = (fds[ix].revents & POLLOUT),
                   $y_Producer_hungup = (fds[ix].revents & POLLHUP),
                   $y_Producer_error = (fds[ix].revents & POLLERR));
        ++ix;
      }
      each_o($$lo_producers, pr_output);
    }
  }
}
