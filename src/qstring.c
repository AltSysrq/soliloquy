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

static qchar zero = 0;
const qchar*const qempty = &zero;
