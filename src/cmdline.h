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
#ifndef CMDLINE_H_
#define CMDLINE_H_

/**
 * Defines a command-line argument. short and long are *unquoted* strings which
 * specify the short (single-char) and long (string) argument forms; words in
 * long *must* be separated by underscores; they will be changed to hyphens for
 * presentation/parsing. If short is a bare hyphen, this argument has no short
 * form.
 *
 * Argspec is either `none` (no quotes), which indicates that the argument
 * takes no parameter, or a variable identifier which is used both in the help
 * string and as the name of the string parameter to the handler.
 *
 * helpstring is a quoted string which explains the function of the argument.
 *
 * This macro both binds the handler to the argument and defines the handler
 * itself. Example:
 *
 * def_cmdline_arg(h, say_hello, name, "Greet the person defined by <name>.") {
 *   printf("Hello, %s\n", name);
 * }
 *
 * The help string for this argument would look something like this:
 *   -h, --say-hello=<name>
 *       Greet the person defined by <name>.
 *
 * The argument could be used as
 *   sol -hworld
 * or
 *   sol --say-hello=world
 */
#define def_cmdline_arg(short, long, argspec, helpstring)       \
  static void handle_##long(string);                            \
  ATSINIT {                                                     \
    bind_cmdline_arg(#short[0], #long, #argspec, handle_##long, \
                     helpstring);                               \
  }                                                             \
  static void handle_##long(string argspec)

/**
 * Defines a command-line argument. You probably want the def_cmdline_arg()
 * macro above.
 *
 * @param shortn The short name of this argument, or '-' for none.
 * @param longn The long name of this argument, hyphens substituted for
 * underscores.
 * @param argspec The name of this argument's parameter, or "none" for none.
 * @param handler The function to call when this argument is processed.
 * @param helpstring The human-readable description of this argument's
 * function.
 */
void bind_cmdline_arg(char shortn, string longn, string argspec,
                      void (*handler)(string), string helpstring);

/**
 * Processes the given command-line arguments, as passed into main().
 *
 * This should only be called by main().
 */
void process_cmdline_args(char** argv, unsigned argc);

#endif /* CMDLINE_H_ */
