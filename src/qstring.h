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
#ifndef QSTRING_H_
#define QSTRING_H_

#include <wchar.h>

/**
 * A qchar represents a single Unicode character (wchar_t) with formatting
 * information. All of Unicode can be represented, except for the 16th astral
 * plane, which spills into the formatting bits. Since this is a private use
 * area, this shouldn't be a problem. (In fact, we can make the program
 * technically correct: The 16th supplementary plane represents members of the
 * Basic Multilingual Plane in reverse video, within the context of this
 * program.)
 *
 * Formatting is divided into foreground colour (mask QC_FG), background colour
 * (mask QC_BG), and attributes (QC_ATTR). Both colours are represented by a
 * quarduple of R (red), G (green), B (blue), and L (luminance, or
 * brightness), one bit each. The RGB (but not L) bits for the foreground
 * colour are inverted, so that a format of 0 results in white text on black
 * background.
 *
 * The attributes are bold, underline, italics, and reverse video, and may be
 * combined arbitrarily.
 *
 * A format of 0 results in plain, white-on-black text. Thus, a wchar_t can be
 * converted to a qchar simply by casting it. (Don't use memcpy() or pointer
 * casts, since sizeof(wchar_t) might != sizeof(qchar).)
 *
 * Note that the application of character formatting is done on a best-effort
 * basis. In particular:
 * - It is generally impossible to make both the foreground and background
 *   bright. When both L bits are set, bold may be set implicitly.
 * - On many terminals, bold implies bright foreground.
 * - On some terminals, bright also implies bold.
 * - Italics are not that commonly available.
 * - Reverse video is performed in software; the hardware attribute is used to
 *   get bright foregrounds without using bold.
 */
typedef unsigned int qchar;

#define QC_FG_R ((qchar)(0x80000000))
#define QC_FG_G ((qchar)(0x40000000))
#define QC_FG_B ((qchar)(0x20000000))
#define QC_FG_L ((qchar)(0x10000000))
#define QC_FG   ((qchar)(0xF0000000))
#define QC_BG_R ((qchar)(0x08000000))
#define QC_BG_G ((qchar)(0x04000000))
#define QC_BG_B ((qchar)(0x02000000))
#define QC_BG_L ((qchar)(0x01000000))
#define QC_BG   ((qchar)(0x0F000000))
#define QC_BOLD ((qchar)(0x00800000))
#define QC_ULIN ((qchar)(0x00400000))
#define QC_ITAL ((qchar)(0x00200000))
#define QC_RVID ((qchar)(0x00100000))
#define QC_ATTR ((qchar)(0x00F00000))
#define QC_FORM ((qchar)(0xFFF00000))
#define QC_CHAR ((qchar)(0x000FFFFF))

#define QC_FG_SHIFT 28
#define QC_BG_SHIFT 24
#define QC_COLOUR_SHIFT 24
#define QC_ATTR_SHIFT 20

/* A "colour byte" is simply the colour part of a qchar, right shifted by
 * QC_COLOUR_SHIFT.
 *
 * This macro allows for easily defining colour bytes as such:
 *   CB(BRIGHT_WHITE,DARK_BLUE) //bright white foreground, dark blue background
 */
#define CB(fg,bg) ((((fg)<<4)^0xE0) | (bg))

// Colour definitions (note that they need to be XORed with 0xE for the
// foreground
#define DARK_BLACK     0x0
#define BRIGHT_BLACK   0x1
#define DARK_BLUE      0x2
#define BRIGHT_BLUE    0x3
#define DARK_GREEN     0x4
#define BRIGHT_GREEN   0x5
#define DARK_CYAN      0x6
#define BRIGHT_CYAN    0x7
#define DARK_RED       0x8
#define BRIGHT_RED     0x9
#define DARK_MAGENTA   0xA
#define BRIGHT_MAGENTA 0xB
#define DARK_YELLOW    0xC
#define BRIGHT_YELLOW  0xD
#define DARK_WHITE     0xE
#define BRIGHT_WHITE   0xF

/**
 * Converts the given qstring to an mwstring by stripping its formatting.
 */
wchar_t* qstrtowstr(const qchar*) __attribute__((malloc));
/**
 * Converts the given wstring to an mqstring with the default (null)
 * formatting.
 */
qchar* wstrtoqstr(const wchar_t*) __attribute__((malloc));
/**
 * Strips the given qchar of any formatting, resulting in a simple character.
 */
static inline wchar_t qchrtowchr(qchar ch) {
  return (wchar_t)(ch & QC_CHAR);
}

/**
 * Returns the length, in characters, of the given qstring.
 */
size_t qstrlen(const qchar*);
/**
 * Returns a mutable copy of the given qstring.
 */
qchar* qstrdup(const qchar*) __attribute__((malloc));
/**
 * Returns a mutable copy of the given wstring.
 *
 * Don't use wcsdup(), since it allocates with malloc().
 */
wchar_t* wstrdup(const wchar_t*) __attribute__((malloc));

/**
 * Searches the given qstring for the given character, ignoring
 * formatting. Returns NULL if not found, or a pointer to the first such
 * character otherwise.
 */
qchar* qstrchr(const qchar*, qchar);
/**
 * Like strlcpy(), but for qstrings.
 */
size_t qstrlcpy(qchar* dst, const qchar* src, size_t sz);
/**
 * Like strlcat(), but for qstrings.
 */
size_t qstrlcat(qchar* dst, const qchar* src, size_t sz);

/**
 * Like strlcpy(), but for wstrings.
 */
size_t wstrlcpy(wchar_t* dst, const wchar_t* src, size_t sz);
/**
 * Like strlcat(), but for wstrings.
 */
size_t wstrlcat(wchar_t* dst, const wchar_t* src, size_t sz);

/**
 * Concatenates the given qstrings into a new qstring.
 */
qchar* qstrap(const qchar*, const qchar*);

/**
 * Concatenates the three given qstrings into a new qstring.
 */
qchar* qstrap3(const qchar*, const qchar*, const qchar*);

/**
 * Concatenates the given qstrings into a new qstring.
 */
qchar* qstrapv(const qchar*const*, unsigned count);

/**
 * Concatenates the given wstrings into a new wstring.
 */
wchar_t* wstrap(const wchar_t*, const wchar_t*);

/**
 * Returns whether the given character is a non-control character, taking into
 * account the existence of virtual key non-characters.
 */
bool is_nc_char(qchar);

/**
 * Returns whether the given pair of characters constitute a word boundary;
 * that is, it returns true if the second parameter is the beginning of a
 * logical word, given the character that precedes it.
 */
bool is_word_boundary(qchar,qchar);

/**
 * The empty qstring.
 */
extern const qchar*const qempty;
#endif /* QSTRING_H_ */
