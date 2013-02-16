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
#include "regex_support.slc"

#include "regex_support.h"

#include <assert.h>
#include <pcre.h>

/*
  Our regular expression data consists primarily of the PCRE structure. We
  also keep track of how many times rx_match() has been called on the
  expression. When it reaches STUDY_THRESH, try to study it to improve
  performance. When it reaches JIT_THRESH, try to compile it to native code.
*/
#define STUDY_THRESH 32
#define JIT_THRESH 256
struct regular_expression {
  pcre* native;
  pcre_extra* aux;
  unsigned use_count;
};

static void finalise_regular_expression(void* rev, void* ignore) {
  regular_expression* re = rev;
  if (re->aux)
    pcre_free_study(re->aux);
  pcre_free(re->native);
}

static regular_expression* alloc_re(void) {
  regular_expression* re = new(regular_expression);
  void* ocd;
  void (*ofn)(void*,void*);
  GC_register_finalizer(re, finalise_regular_expression, NULL,
                        &ofn, &ocd);

  return re;
}

/*
  SYMBOL: $u_invalid_regular_expression
    Rollback type if regular expression compilation fails and the error parm to
    rx_comple() was NULL.
 */
regular_expression* rx_compile(wstring pattern, string* error_out) {
  static const unsigned char* unicode_table;
  if (!unicode_table)
    unicode_table = pcre_maketables();

  string pattern8 = wstrtocstr(pattern);
  string* error = (error_out ?: &$s_rollback_reason);
  int dont_care;
  pcre* re = pcre_compile(pattern8, PCRE_UTF8, error, &dont_care,
                          unicode_table);
  if (!re) {
    if (error_out) {
      return NULL;
    } else {
      $v_rollback_type = $u_invalid_regular_expression;
      tx_rollback();
    }
  }

  regular_expression* ret = alloc_re();
  ret->native = re;
  return ret;
}

unsigned rx_match(regular_expression* this, wstring str,
                  wstring* groups, unsigned max_groups) {
  string str8 = wstrtocstr(str);

  // Increment use count, and perform studying/compiling if it hits those
  // threshholds
  ++this->use_count;
  if (this->use_count == STUDY_THRESH) {
    string dont_care;
    this->aux = pcre_study(this->native, 0, &dont_care);
  } else if (this->use_count == JIT_THRESH) {
#ifdef PCRE_STUDY_JIT_COMPILE
    if (this->aux)
      pcre_free_study(this->aux);

    string dont_care;
    this->aux = pcre_study(this->native, PCRE_STUDY_JIT_COMPILE, &dont_care);
    // Set up a 1MB stack for the JIT code
    if (this->aux) {
      static void* jitstack;
      if (!jitstack) {
        jitstack = pcre_jit_stack_alloc(32*1024, 1024*1024);
      }

      if (jitstack) {
        pcre_assign_jit_stack(this->aux, NULL, jitstack);
      }
    }
#endif
  }

  // Add 5 to the number of groups to give libpcre some more working space (eg,
  // for backreferences).
  int group_pairs[3 * (max_groups+5)];
  memset(group_pairs, -1, sizeof(group_pairs));
  int result = pcre_exec(this->native, this->aux, str8, strlen(str8), 0,
                         0, group_pairs, lenof(group_pairs));

  if (result < 0) return 0; // No match

  // Match, extract groups
  // Rather irritatingly, PCRE won't tell us how many groups were actually
  // found if it ran out of scratch space within the buffer we gave it, so
  // count manually.
  unsigned cnt = 0;
  while (cnt < lenof(group_pairs)/3 && group_pairs[cnt*3] != -1) {
    if (cnt < max_groups) {
      unsigned sz = group_pairs[cnt*3+1] - group_pairs[cnt*3+0];
      char buff[sz+1];
      buff[sz] = 0;
      memcpy(buff, str8 + group_pairs[cnt*3+0], sz);

      groups[cnt] = cstrtowstr(buff);
    }

    ++cnt;
  }

  // Success.
  // cnt should always be > 0 since there was *some* match.
  assert(cnt);
  return cnt;
}
