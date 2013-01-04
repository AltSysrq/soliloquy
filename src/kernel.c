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

#include "kernel.slc"

#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <poll.h>
#include <errno.h>

// classes $c_Consumer $c_Producer

static void handle_sigchld(int, siginfo_t*, void*);
static void handle_quit(int);
static void handle_fatal(int);

ATSTART(initialise_kernel, STATIC_INITIALISATION_PRIORITY) {
  $$lo_tasks = NULL;
  $$lo_consumers = NULL;
  $$lo_producers = NULL;

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

static void handle_sigchld(int sigchld, siginfo_t* info, void* unknown) {
  $i_async_sygnal = sigchld;
  $y_signal_is_synchronous = true;
  $i_sigchld_code = info->si_code;
  $i_sigchld_pid = info->si_pid;
  $i_sigchld_status = info->si_status;

  $y_is_handling_signal = true;
  $f_sigchld();
  $y_is_handling_signal = false;
}

static void handle_quit(int which) {
  $i_async_sygnal = which;
  $y_signal_is_synchronous = (which != SIGQUIT);
  $y_is_handling_signal = true;

  $f_save_the_world();
  $f_graceful_exit();
  exit(0);
}

static void handle_fatal(int which) {
  $i_async_sygnal = which;
  $y_signal_is_synchronous = true;
  $y_is_handling_signal = true;

  $f_save_the_world();
  $f_die_gracelessly();

  // Die with the same signal, the way the OS would like
  signal(which, SIG_DFL);
  raise(which);
}


defun($h_Consumer) {
  $$lo_consumers = cons_o($o_Consumer, $$lo_consumers);
}

defun($h_Consumer_destroy) {
  $$lo_consumers = lrm_o($$lo_consumers, $o_Consumer);
}

defun($h_Producer) {
  $$lo_producers = cons_o($o_Producer, $$lo_producers);
}

defun($h_Producer_destroy) {
  $$lo_producers = lrm_o($$lo_producers, $o_Producer);
}

advise_before($h_kernel_main) {
  $$y_keep_running = true;
}

defun($h_kernel_main) {
  while ($$y_keep_running)
    $f_kernel_cycle();
}

defun($h_kernel_cycle) {
  // Reset poll duration variables, then run one cycle for all tasks. If any
  // need to altern the poll duration, they can do so.
  $i_kernel_poll_duration_ms = 0x7FFFFFFF;
  $y_kernel_poll_infinite = true;
  $f_run_tasks();

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
