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
  TITLE: Formatted Wide-Character Manipulation Functions
  OVERVIEW: Implementation details; just read qstring.h.
*/

// While we have no symbols, include the slc file for consistency.
#include "qstring.slc"

#include <ctype.h>
#include <wctype.h>

mwstring qstrtowstr(qstring src) {
  size_t sz = qstrlen(src)+1;
  mwstring dst = gcalloc(sizeof(wchar_t)*sz);

  for (size_t i = 0; i < sz; ++i)
    dst[i] = qchrtowchr(src[i]);

  return dst;
}

mqstring wstrtoqstr(wstring src) {
  size_t sz = wcslen(src) + 1;
  mqstring dst = gcalloc(sizeof(qchar)*sz);
  if (sizeof(wchar_t) == sizeof(qchar))
    memcpy(dst, src, sz*sizeof(qchar));
  else
    for (size_t i = 0; i < sz; ++i)
      dst[i] = (qchar)src[i];

  return dst;
}

size_t qstrlen(qstring str) {
  size_t ret = 0;
  while (*str++) ++ret;
  return ret;
}

mwstring wstrdup(wstring src) {
  size_t sz = wcslen(src) + 1;
  mwstring dst = gcalloc(sizeof(wchar_t)*sz);
  memcpy(dst, src, sizeof(wchar_t)*sz);
  return dst;
}

mqstring qstrdup(qstring src) {
  size_t sz = qstrlen(src) + 1;
  mqstring dst = gcalloc(sizeof(qchar)*sz);
  memcpy(dst, src, sizeof(qchar)*sz);
  return dst;
}

mqstring qstrchr(qstring haystack_, qchar needle) {
  mqstring haystack = (mqstring)haystack_;
  needle &= QC_CHAR;

  while (*haystack && (*haystack & QC_CHAR) != needle)
    ++haystack;

  return *haystack? haystack : NULL;
}

size_t qstrlcpy(mqstring dst, qstring src, size_t maxsz) {
  size_t srclen = qstrlen(src);
  if (!maxsz) return srclen;
  size_t len = (srclen > maxsz-1? maxsz-1 : srclen);
  memcpy(dst, src, sizeof(qchar)*len);
  memset(dst+len, 0, sizeof(qchar)*(maxsz-len));
  return srclen;
}

size_t qstrlcat(mqstring dstbase, qstring src, size_t maxsz) {
  size_t srclen = qstrlen(src);
  if (!maxsz) return srclen;

  mqstring dst = dstbase;
  while (*dst++ && maxsz--);
  --dst;

  size_t len = (srclen > maxsz-1? maxsz-1 : srclen);
  memcpy(dst, src, len*sizeof(qchar));
  memset(dst+len, 0, sizeof(qchar)*(maxsz-len));

  return len + (dst-dstbase);
}

size_t wstrlcpy(mwstring dst, wstring src, size_t maxsz) {
  size_t srclen = wcslen(src);
  if (!maxsz) return srclen;
  size_t len = (srclen > maxsz-1? maxsz-1 : srclen);
  memcpy(dst, src, sizeof(wchar_t)*len);
  memset(dst+len, 0, sizeof(wchar_t)*(maxsz-len));
  return srclen;
}

size_t wstrlcat(mwstring dstbase, wstring src, size_t maxsz) {
  size_t srclen = wcslen(src);
  if (!maxsz) return srclen;

  mwstring dst = dstbase;
  while (*dst++ && maxsz--);
  --dst;

  size_t len = (srclen > maxsz-1? maxsz-1 : srclen);
  memcpy(dst, src, len*sizeof(wchar_t));
  memset(dst+len, 0, sizeof(wchar_t)*(maxsz-len));

  return len + (dst-dstbase);
}

mqstring qstrap(qstring a, qstring b) {
  size_t alen = qstrlen(a), blen = qstrlen(b);
  mqstring dst = gcalloc(sizeof(qchar)*(alen+blen+1));
  memcpy(dst, a, sizeof(qchar)*alen);
  memcpy(dst+alen, b, sizeof(qchar)*blen);
  //Final qchar was initialised to zero by gcalloc().
  return dst;
}

mwstring wstrap(wstring a, wstring b) {
  size_t alen = wcslen(a), blen = wcslen(b);
  mwstring dst = gcalloc(sizeof(wchar_t)*(alen+blen+1));
  memcpy(dst, a, sizeof(wchar_t)*alen);
  memcpy(dst+alen, b, sizeof(wchar_t)*blen);
  return dst;
}

mqstring qstrap3(qstring a, qstring b, qstring c) {
  qstring v[3];
  v[0] = a;
  v[1] = b;
  v[2] = c;
  return qstrapv(v, 3);
}

mqstring qstrapv(const qstring* v, unsigned cnt) {
  size_t vlen[cnt];
  size_t len = 0;
  for (unsigned i = 0; i < cnt; ++i)
    len += vlen[i] = qstrlen(v[i]);

  mqstring dst = gcalloc(sizeof(qchar)*(len + 1));
  mqstring d = dst;
  for (unsigned i = 0; i < cnt; ++i) {
    memcpy(d, v[i], vlen[i]*sizeof(qchar));
    d += vlen[i];
  }

  return dst;
}

bool is_nc_char(qchar q) {
  if (q & (1<<31)) return false;
  if (q < L' ') return false;
  if (q == 0x7F) return false;
  return true;
}

bool is_word_boundary(qchar a, qchar b) {
  // NUL characters are used to indicate beginning/end of string; that is
  // always a word boundary.
  if (!a || !b) return true;

  /* A character is a word boundary if it is alphanumeric and is preceded by a
   * non-alphanumeric character, or if it is uppercase and is preceded by a
   * non-uppercase character.
   */
  if (iswalnum(b)) {
    if (!iswalnum(a)) return true;
    if (iswupper(b) && !iswupper(a)) return true;
  }

  return false;
}

static const qchar space[2] = { L' ', 0 };
const qchar*const qempty = space+1;
const qchar*const qspace = space;

