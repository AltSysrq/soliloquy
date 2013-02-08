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
#include "pattern.slc"

/*
  TITLE: Pattern Matching Utilities
  OVERVIEW: Provides functions to perform various types of pattern matching,
    for purposes such as autocomplete and searching.
 */


/*
  SYMBOL: $f_pseudo_steno_expand $w_pseudo_steno_expand
    Attempts to perform "pseudo-stenographic" expansion of
    $w_pseudo_steno_expand, by querying $p_pseudo_steno_expand_enumerator. If
    the search is successful, $w_pseudo_steno_expand is set to the expanded
    string. Otherwise, it is set to NULL.
    --
    Pseudo-stenographic expansion is performed as follows. Given an input and a
    set of possible completions, the completions are first narrowed to a list
    of candidates according to those which satisfy all of the following
    criteria:
    - The candidate begins with the same character as the input.
    - The input's character sequence is a subsequence of that of the candidate.
    - It is possible to match this subsequence to the candidate such that no
      word boundaries occur between two characters in the input which
      themselves did not constitute a word boundary.
    The last criterion means that, given an input of "slq", "soliloquy" will be
    a candidate, whereas "sillyIraq" is not.
    Once a list of candidate matches is obtained, a match is selected according
    to the following criteria, considered in the order given:
    - The candidate(s) with the fewest word boundaries.
    - The shortest candidate(s).
    - The candidate with the latest occurrance of inserted characters. This
      criterion is reevaluated starting further into the string until only one
      candidate remains, or the end of the string remains (which means that
      the candidates are equal strings).
    --
    There is a concept of "subsequences" (see
    $p_pseudo_steno_expand_enumerator); if a match is found for a subsequence,
    subsequent subsequences will not be searched. This can be used, for
    example, to give precedence to identifiers in code proper over words which
    happen to occur in comments.
    --
    Inputs of zero or one characters will always fail to match.
    --
    This function uses a somewhat different definition of "word boundary" when
    examining the input (but not the candidates) --- consecutive uppercase
    characters shall also be considered to constitute word boundaries. This is
    to allow for abbreviations like "DoBF" to mean "DocumentBuilderFactory",
    which is esspecially useful for environments like Java.

  SYMBOL: $p_pseudo_steno_expand_enumerator $x_pseudo_steno_expand_enumerator
  SYMBOL: $w_pseudo_steno_expand_enumerator $y_pseudo_steno_expand_enumerator
    $p_pseudo_steno_expand_enumerator is a function pointer of type
    `void (*)(void)`. It is called repeadedly by $f_pseudo_steno_expand to
    enumerate possible expansions of the expansion query. It must set
    $w_pseudo_steno_expand_enumerator to either an enumerated string, or NULL
    to indicate end of subsequence. It SHOULD only return strings whose first
    character is equal to $x_pseudo_steno_expand_enumerator, but MAY return
    strings which do not fit this criterion. It MUST NOT return an empty
    string. It must set $y_pseudo_steno_expand_enumerator to false to indicate
    that it cannot enumerate any more strings; in this case, the value in
    $w_pseudo_steno_expand_enumerator will be ignored.
    --
    The separation of end-of-subsequence (by returning NULL) and
    end-of-enumeration (by setting $y_pseudo_steno_expand_enumerator to false)
    allow for separating possible completions into multiple levels of
    priority. The function will only be polled for more data after the end of a
    subsequence if no completion could be found in the previous.
    --
    The contents of $p_pseudo_steno_expand_enumerator may be changed during
    execution; this will have the expected result of changing the function
    called.
    --
    $w_pseudo_steno_expand MUST NOT be altered by this function.
 */
defun($h_pseudo_steno_expand) {
  //TODO
}
