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
#include <wctype.h>

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
    - The candidate with the latest occurrance of inserted characters.
    - The candidate whose first inserted character is more common, ignoring
      inserted characters between the candidates which are equal. Character
      commonness is a static table of English letter probabilities (noting
      that most programming occurs in English). This rule takes into account
      the fact that people are more likely to be explicit about the less
      common characters, while expecting common ones (like 'e') to be
      inserted. Non-ASCII leters are not handled here; they are considered to
      be less frequent than any ASCII letter.
    - Choose the one which comes first, sorting according to Unicode code
      points. This is simply a tie-breaker for consistency.
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
    $w_pseudo_steno_expand_enumerator MUST be NULL.
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
static bool is_alternate_word_boundary(wchar_t, wchar_t);
static bool starts_with_same_char(wstring, wstring);
static bool is_supersequence_of_input(wstring candidate, wstring input);
static bool can_fit_word_boundary_rule(wstring candidate, wstring input);
static wstring select_shorter(wstring, wstring);
static wstring select_with_fewer_word_boundaries(wstring, wstring);
static wstring select_with_latest_insertions(wstring,wstring, wstring input);
static wstring select_with_most_common_insertion(wstring,wstring, wstring input);
static wstring select_asciibetically_first(wstring,wstring);
static wstring enumerate_next_expansion(void);
defun($h_pseudo_steno_expand) {
  if (wcslen($w_pseudo_steno_expand) < 2) {
    $w_pseudo_steno_expand = NULL;
    return;
  }

  wstring best = NULL;
  $y_pseudo_steno_expand_enumerator = true;
  while ($y_pseudo_steno_expand_enumerator && !best) {
    wstring candidate;
    while ((candidate = enumerate_next_expansion())) {
      wstring input = $w_pseudo_steno_expand;
      if (starts_with_same_char(candidate, input) &&
          is_supersequence_of_input(candidate, input) &&
          can_fit_word_boundary_rule(candidate, input)) {
        if (!best)
          best = candidate;
        else
          best =
            select_shorter(best, candidate) ?:
            select_with_fewer_word_boundaries(best, candidate) ?:
            select_with_latest_insertions(best, candidate, input) ?:
            select_with_most_common_insertion(best, candidate, input) ?:
            select_asciibetically_first(best, candidate);
      }
    }
  }

  $w_pseudo_steno_expand = best;
}

static wstring enumerate_next_expansion(void) {
  $x_pseudo_steno_expand_enumerator = $w_pseudo_steno_expand[0];
  $y_pseudo_steno_expand_enumerator = true;
  $w_pseudo_steno_expand_enumerator = NULL;
  ((void (*)(void))$p_pseudo_steno_expand_enumerator)();
  return $w_pseudo_steno_expand_enumerator;
}


static bool is_alternate_word_boundary(wchar_t a, wchar_t b) {
  return is_word_boundary(a,b) ||
    (iswupper(a) && iswupper(b));
}

static bool starts_with_same_char(wstring a, wstring b) {
  return a[0] == b[0];
}

static bool is_supersequence_of_input(wstring candidate, wstring input) {
  while (*candidate && *input) {
    // Move candidate forward if it matches the current character of input
    if (*candidate++ == *input)
      ++input;
  }

  if (*input)
    // This candidate is empty; the input is not, so there can be no match
    return false;

  // Either both are empty now, or only candidate is (meaning that we just
  // insert more characters beyond the end of candidate), so this is a match
  return true;
}

deftest(is_supersequence_of_input) {
  assert(is_supersequence_of_input(L"foo", L"foo"));
  assert(is_supersequence_of_input(L"foo", L"fo"));
  assert(is_supersequence_of_input(L"foobar", L"fb"));
  assert(is_supersequence_of_input(L"fooBar", L"fa"));
  assert(is_supersequence_of_input(L"zoooooom", L"zoom"));
  assert(is_supersequence_of_input(L"soliloquy", L"slq"));
  assert(is_supersequence_of_input(L"oo", L"o"));
  assert(is_supersequence_of_input(L"o", L"o"));

  assert(!is_supersequence_of_input(L"zar", L"foo"));
  assert(!is_supersequence_of_input(L"eeeee", L"eeeeee"));
  assert(!is_supersequence_of_input(L"zoom", L"zooooo"));
  assert(!is_supersequence_of_input(L"fo", L"fa"));
}

static bool can_fit_word_boundary_rule(wstring candidate, wstring input) {
  // Base case: strings exhausted
  if (!*input || !*candidate)
    return *input == *candidate;

  /*
    Invariant: input[1] and candidate[1] have defined values.

    Invariant: We have already matched input[0]; candidate[0] is either
    matched or already rejected.
      Thus, input is moved forward whenever a potential match is found between
      input[1] and candidate[1].

    There may be multiple possible ways to distribute the matches, so use
    recursion to "fork" the check in case we need to backtrack.

    If the input pair is not a word boundary, the candidate pair must not be a
    word boundary. If the input pair is a word boundary (using the alternate
    rule), we don't care about the candidate pair.

    If there is a candidate-input match, we don't need to worry about word
    boundaries, because any boundary indicated by candidate[1] and which also
    matches input[1] would cause the input pair to be a word boundary.
   */
  bool input_pair_is_word_boundary =
    is_alternate_word_boundary(input[0], input[1]);
  while (*candidate) {
    if (candidate[1] == input[1] &&
        can_fit_word_boundary_rule(candidate+1, input+1))
      return true;

    // No match or backtracking

    if (!input_pair_is_word_boundary &&
        is_word_boundary(candidate[0], candidate[1]))
      // This possibility fails the rule
      return false;

    // No match, but the insertion here is OK
    ++candidate;
  }

  // Reached end of candidate before end of input; the candidate does not
  // match, at least with the choices made so far
  return false;
}

deftest(can_fit_word_boundary_rule) {
  assert(can_fit_word_boundary_rule(L"foo", L"f"));
  assert(can_fit_word_boundary_rule(L"foobar", L"fb"));
  assert(can_fit_word_boundary_rule(L"fooBar", L"fB"));
  assert(can_fit_word_boundary_rule(L"FooBar", L"FB"));
  assert(can_fit_word_boundary_rule(L"abCxCbx", L"aCbx"));
  assert(!can_fit_word_boundary_rule(L"abCd", L"abd"));
  assert(!can_fit_word_boundary_rule(L"eeeeeeXf", L"ef"));
}

wstring select_shorter(wstring a, wstring b) {
  size_t alen = wcslen(a), blen = wcslen(b);

  if (alen < blen)
    return a;
  else if (blen < alen)
    return b;
  else
    return NULL;
}

deftest(select_shorter) {
  assert(!select_shorter(L"foo", L"bar"));
  assert(!wcscmp(L"foo",
                 select_shorter(L"foo", L"foobar")));
  assert(!wcscmp(L"foo",
                 select_shorter(L"foobar", L"foo")));
}

static unsigned count_word_boundaries(wstring str) {
  unsigned cnt = 0;
  while (*str) {
    if (is_word_boundary(str[0], str[1]))
      ++cnt;

    ++str;
  }

  return cnt;
}

wstring select_with_fewer_word_boundaries(wstring a, wstring b) {
  unsigned acnt = count_word_boundaries(a), bcnt = count_word_boundaries(b);

  if (acnt < bcnt)
    return a;
  else if (bcnt < acnt)
    return b;
  else
    return NULL;
}

deftest(select_with_fewer_word_boundaries) {
  assert(!select_with_fewer_word_boundaries(L"foo", L"foo"));
  assert(!wcscmp(L"foobar",
                 select_with_fewer_word_boundaries(L"foobar",
                                                   L"fooBar")));
  assert(!wcscmp(L"foobar",
                 select_with_fewer_word_boundaries(L"fooBar",
                                                   L"foobar")));
}

wstring select_with_latest_insertions(wstring a_orig, wstring b_orig,
                                      wstring input) {
  wstring a = a_orig, b = b_orig;

  while (*a && *b && *input && *a == *input && *b == *input)
    ++a, ++b, ++input;

  // We have either encountered the end of one of the strings, or one of them
  // needs an insertion.
  if (*a != *input && *b != *input)
    // Tie (insertions at the same point)
    return NULL;

  if (*a != *input)
    return b_orig;
  else //*b != *input
    return a_orig;
}

deftest(select_with_latest_insertions) {
  assert(!select_with_latest_insertions(L"Axe", L"Axx", L"Ax"));
  assert(!wcscmp(L"Axe",
                 select_with_latest_insertions(L"Axe",
                                               L"Annex",
                                               L"Ax")));
  assert(!wcscmp(L"Axe",
                 select_with_latest_insertions(L"Annex",
                                               L"Axe",
                                               L"Ax")));
}

/*
  SYMBOL: $w_character_freqs
    A list of characters for frequency consideration during
    pseudo-stenographich expansions, sorted by descending frequency.
    Source: http://en.wikipedia.org/wiki/Letter_frequency
 */
STATIC_INIT_TO($w_character_freqs,
               L"etaoinshrdlcumwfgypbvkjxqzETAOINSHRDLCUMWFGYPBVKJXQZ-_");

wstring select_with_most_common_insertion(wstring a_orig, wstring b_orig,
                                           wstring input) {
  wstring a = a_orig, b = b_orig;

  while (*a && *b && *input && ((*a == *input && *b == *input) || *a == *b)) {
    if (*a == *input && *b == *input)
      ++input;
    ++a, ++b;
  }

  wstring aocc = wcschr($w_character_freqs, *a),
          bocc = wcschr($w_character_freqs, *b);
  if (!aocc && !bocc)
    return NULL;
  if (!aocc)
    return b_orig;
  if (!bocc)
    return a_orig;

  if (aocc < bocc)
    return a_orig; //*a is the more common character
  else if (bocc < aocc)
    return b_orig;
  else
    return NULL;
}

deftest(select_with_most_common_insertion) {
  assert(!select_with_most_common_insertion(L"foo", L"foo", L"f"));
  assert(!wcscmp(L"fee",
                 select_with_most_common_insertion(
                   L"foo", L"fee", L"f")));
  assert(!wcscmp(L"fee",
                 select_with_most_common_insertion(
                   L"fee", L"foo", L"f")));
}

wstring select_asciibetically_first(wstring a, wstring b) {
  int res = wcscmp(a,b);
  if (res < 0)
    return a;
  else //res >=0
    return b;

  //If res==0, they are equal, so it doesn't matter
}

deftest(select_asciibetically_first) {
  assert(!wcscmp(L"bar", select_asciibetically_first(L"foo", L"bar")));
  assert(!wcscmp(L"bar", select_asciibetically_first(L"bar", L"foo")));
}

/*
  SYMBOL: $c_Pattern
    Encapsulates data used for Soliloquy's pattern matching. Unlike some other
    editors, Soliloquy defaults to literal string matching, and can easily be
    switched to regeular expressions by an embedded control character. When the
    object is constructed, the constructor attempts to compile the pattern in
    $w_Pattern_pattern. If that fails, the current transaction is rolled back.

  SYMBOL: $w_Pattern_pattern
    The source for the pattern to match against. The pattern rules are as follows:
    - If the pattern contains the character ^R, the first ^R is deleted and the
      entire resulting string treated as a verbatim regular expression.
    - Otherwise, the pattern is split into terms on whitespace. Empty terms are
      deleted. Terms work as follows
      - If the term contains a ^A, it is anchored to the begining of the
        string and the first ^A deleted; if it contains a ^Z, it is anchored to
        the end of the string and the ^Z deleted; otherwise, it is unanchored.
      - At most one term may be anchored to the beginning, and one to the end;
        if this is violated, it is an error.
      - For a string to match, all terms must be found within the string, and
        *may* overlap. A term anchored to the beginning must begin on the first
        non-whitespace character in the string; similarly, a term anchored to
        the end must terminate on the last non-whitespace character.
    - The null pattern matches everything.

  SYMBOL: $w_Pattern_begin_anchor $w_Pattern_end_anchor
    Literal strings to match at the beginning and end of the input string for a
    pattern, respectively. NULL indicates no requirement.

  SYMBOL: $lw_Pattern_terms
    Literal strings to match anywhere within the input strings for a pattern.
 */
defun($h_Pattern) {
  wstring control_r = wcschr($w_Pattern_pattern, L'R' - L'@');
  if (control_r) {
    // Regex --- TODO
    $s_rollback_reason = "Regexen not yet supported";
    $v_rollback_type = $u_Pattern;
    tx_rollback();
  }

  mwstring token, state;
  for (token = wcstok(wstrdup($w_Pattern_pattern), L" \t\v\r\n", &state);
       token;
       token = wcstok(NULL, L" \t\v\r\n", &state)) {
    if (!*token) continue; //Ignore empty term

    mwstring control;
    if ((control = wcschr(token, L'A' - L'@'))) {
      wmemmove(control, control+1, wcslen(control) /* +1-1*/ );
      // Do nothing if this makes the string empty
      if (!*token) continue;
      //Anchored to beginning
      if ($w_Pattern_begin_anchor) {
        $s_rollback_reason = "More than one beginning anchor in pattern.";
        $v_rollback_type = $u_Pattern;
        tx_rollback();
      } else {
        $w_Pattern_begin_anchor = wstrdup(token);
      }
    } else if ((control = wcschr(token, L'Z' - L'@'))) {
      wmemmove(control, control+1, wcslen(control));
      // Do nothing if this makes the string empty
      if (!*token) continue;
      //Anchored to end
      if ($w_Pattern_end_anchor) {
        $s_rollback_reason = "More than one ending anchor in pattern.";
        $v_rollback_type = $u_Pattern;
        tx_rollback();
      } else {
        $w_Pattern_end_anchor = wstrdup(token);
      }
    } else {
      // No anchoring
      lpush_w($lw_Pattern_terms, wstrdup(token));
    }
  }
}

/*
  SYMBOL: $f_Pattern_matches $y_Pattern_matches
    Tests whether the string in $w_Pattern_input matches this Pattern, setting
    $y_Pattern_matches to indicate the result. Note that $w_Pattern_input will
    be modified by this call (only the pointer; the string itself is immutable)
    to point to the first non-whitespace character.

  SYMBOL: $w_Pattern_input
    Input string to test against this pattern. The pointer itself may be
    modified.
 */
defun($h_Pattern_matches) {
  // Advance input string beyond whitespace
  while (*$w_Pattern_input && iswspace(*$w_Pattern_input))
    ++$w_Pattern_input;

  if ($p_Pattern_regex) {
    // Not yet supported
    $y_Pattern_matches = false;
  } else {
    if (!*$w_Pattern_input) {
      // Null input; can only match null pattern
      $y_Pattern_matches = 
        !$w_Pattern_begin_anchor &&
        !$w_Pattern_end_anchor &&
        !$lw_Pattern_terms;
      return;
    }

    // Check beginning anchor if required
    if ($w_Pattern_begin_anchor) {
      if (wcsncmp($w_Pattern_input, $w_Pattern_begin_anchor,
                  wcslen($w_Pattern_input))) {
        $y_Pattern_matches = false;
        return;
      }
    }

    // Check end anchor if required
    if ($w_Pattern_end_anchor) {
      if (wcslen($w_Pattern_end_anchor) > wcslen($w_Pattern_input)) {
        // Anchor is longer than string, so it can't match
        $y_Pattern_matches = false;
        return;
      }

      // Starting from the end, locate the first non-whitespace character, then
      // move back by the number of characters in the anchor
      wstring input = $w_Pattern_input + wcslen($w_Pattern_input);
      while (iswspace(*input) && input != $w_Pattern_input) {
        --input;
      } 

      // Ensure there is enough space for the anchor to possibly match
      if (((unsigned)(input - $w_Pattern_input)) <
          wcslen($w_Pattern_end_anchor)) {
        $y_Pattern_matches = false;
        return;
      }

      input -= wcslen($w_Pattern_end_anchor);
      // See whether the anchor actually matches
      if (wcsncmp(input,$w_Pattern_end_anchor,wcslen($w_Pattern_end_anchor))) {
        $y_Pattern_matches = false;
        return;
      }
    }

    // Check for other terms
    $y_Pattern_matches =
      every_w($lw_Pattern_terms,
              lambdab((wstring term), wcsstr($w_Pattern_input, term)));
  }
}
