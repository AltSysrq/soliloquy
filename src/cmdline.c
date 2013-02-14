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
#include "cmdline.slc"

#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

typedef struct {
  char shortn;
  string longn;
  string parm;
  string help;
  void (*handler)(string);
} argspec;

void bind_cmdline_arg(char shortn, string longn, string parm,
                      void (*handler)(string), string help) {
  argspec arg = {
    .shortn = (shortn == '-'? 0 : shortn),
    .longn = longn,
    .parm = (strcmp(parm, "none")? parm : NULL),
    .help = help,
    .handler = handler,
  };

  lpush_p($$lp_command_line_args, newdup(&arg));
}

static string uscore_to_hyphens(string in) {
  mstring out = gcstrdup(in);
  for (mstring m = out; *m; ++m)
    if (*m == '_')
      *m = '-';

  return out;
}

/*
  SYMBOL: $f_cmdline_arguments_processed
    Called after command-line arguments (ie, options) have been processed, and
    $ls_cmdline_args has been set to the remaining non-option arguments, if
    any.

  SYMBOL: $ls_cmdline_args
    After command-line options have been processed, set to a list of strings on
    the command-line which were not consumed by options.
 */
static void handle_help(string);
void process_cmdline_args(char** argv, unsigned argc) {
  unsigned shortlen = 0, nlong = llen_p($$lp_command_line_args);
  // Count the number of short arguments, counting parameterised arguments
  // as two (since they are followed by colons)
  each_p($$lp_command_line_args,
         (void(*)(void*))
         lambdav((argspec* arg),
                 if (arg->shortn)
                   shortlen += (arg->parm? 2:1)));

  char shortopts[shortlen+1];
  struct option longopts[nlong+1];
  void (*handlers[nlong])(string);
  shortopts[shortlen] = 0;
  memset(longopts+nlong, 0, sizeof(struct option));

  unsigned shortix = 0, longix = 0;
  for (list_p curr = $$lp_command_line_args; curr; curr = curr->cdr) {
    argspec* arg = curr->car;
    if (arg->shortn) {
      shortopts[shortix++] = arg->shortn;
      if (arg->parm)
        shortopts[shortix++] = ':';
    }

    longopts[longix] = (struct option){
      .name = uscore_to_hyphens(arg->longn),
      .has_arg = !!arg->parm,
      .val = 1,
      .flag = NULL,
    };
    handlers[longix] = arg->handler;
    ++longix;
  }

  int ret;
  unsigned foundix;
  while (-1 != (ret = getopt_long(argc, argv,
                                  shortopts,
                                  longopts, (signed*)&foundix))) {
    if (ret == '?')
      handle_help(NULL);

    handlers[foundix](optarg);
  }

  // Done parsing options; the rest of argv are input files
  unsigned start = (unsigned)optind;
  for (unsigned i = argc; i > start; --i)
    lpush_s($ls_cmdline_args, argv[i-1]);
  $f_cmdline_arguments_processed();
}

def_cmdline_arg(h, help, none,
                "Display help message and exit.") {
  printf("Usage: sol [options] [file [...]]\n");
  printf("The supported options are listed below. Arguments mandatory for long"
         "\narguments are mandatory for short arguments too.\n");
  for (list_p curr = $$lp_command_line_args; curr; curr = curr->cdr) {
    argspec* arg = curr->car;
    printf("  ");
    if (arg->shortn)
      printf("-%c, ", arg->shortn);
    printf("--%s", uscore_to_hyphens(arg->longn));

    if (arg->parm)
      printf("=<%s>", arg->parm);
    printf("\n\t%s\n", arg->help);
  }

  exit(255);
}
