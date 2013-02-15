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
#ifndef REGEX_SUPPORT_H_
#define REGEX_SUPPORT_H_

/**
 * Opaque type representing a compiled regular expression. Instances are
 * allocated on the heap (with the GC), and automatically destroy their
 * back-end object when finalised.
 */
typedef struct regular_expression regular_expression;

/**
 * Attempts to compile the given string into a regular expression. If
 * successful, returns the regular expression object. Otherwise, behaviour
 * depends on the value of error. If it is NULL, the current transaction is
 * rolled back; otherwise, it is set to a string describing why the pattern
 * could not be compiled, and NULL is returned.
 */
regular_expression* rx_compile(wstring, wstring* error);

/**
 * Matches the given regular expression to the given string. Returns the number
 * of groups which matched (0 means no match; >=1 is match). Up to max_pairs*2
 * integers will be written to *group_pairs, indicating
 * inclusive-begin/exclusive-end pairs for each capturing group (where group 0
 * is the whole string which matched the pattern).
 *
 * Be aware that group_pairs MUST be a valid pointer unless max_pairs is zero,
 * and that the return value may be greater than max_pairs. Values within
 * group_pairs beyond min(max_pairs, return_value) will not be altered.
 */
unsigned rx_match(regular_expression*, wstring,
                  unsigned* group_pairs, unsigned max_pairs);

#endif /* REGEX_SUPPORT_H_ */
