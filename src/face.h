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
#ifndef FACE_H_
#define FACE_H_

/**
 * A face is an integer which describes how to modify a qchar to achieve a
 * desired effect. A face is divided into a 16-bit AND-NOT mask and a 16-bit
 * XOR mask, which is applied to the upper 12 bits of the qchar (ie, the format
 * part), the AND-NOT mask followed by the XOR mask. This allows the face to
 * specify any modification to any format bit: keep, clear, set, or invert.
 *
 * The no-op face is zero.
 */
typedef unsigned int face;

#define FACE_AND_MASK 0xFFF00000u
#define FACE_XOR_MASK 0x0000FFF0u
#define FACE_AND_SHIFT 0
#define FACE_XOR_SHIFT 16

/**
 * Parses the given string into a face, which it returns.
 *
 * The string is composed of zero or more alterations. An alteration begins
 * with one of the following characters:
 *   + Force bits set
 *   - Force bits clear
 *   ! Toggle bits
 *   = Keep bits
 *   * Clear bit family, then force specific bits set
 *
 * Following that character is the bits to alter. This is one of the following:
 *   fC   Foreground colour C, where C is one of the colours listed below.
 *        This function hides the fact that the foreground in qchars is
 *        inverted; specifying +fr does mean red, not cyan.
 *   bC   Background colour C, where C is one of the colours listed below.
 *   B    Bold
 *   I    Italic
 *   U    Underline
 *   X    Reverse video
 *
 * The supported colours are:
 *   k Black            K Bright Black (dark grey)
 *   r Red              R Bright Red
 *   y Yellow/brown     Y Bright Yellow
 *   g Green            G Bright Green
 *   c Cyan             C Bright Cyan
 *   b Blue             B Bright Blue
 *   m Magenta          M Bright Magenta
 *   w White (grey)     W Bright White
 *   L Luminance (ie, toggle/set brightness)
 *
 * The bits are in three families: Foreground colour, background colour, and
 * attributes.
 *
 * The keep (=) alteration is not useful for this function, but is for
 * mkface_of().
 *
 * Example: *fr+U-B!X
 *   Sets foreground colour to red (without altering luminance), set underline,
 *   clear bold, toggle reverse video. (And preserve background colour and
 *   luminance, as well as italic.)
 *
 * This function does not check for conflicts in the string; specifying
 * "+fr-fy" will force the foreground to blue or black, since the latter
 * operation cancels out the former.
 *
 * If an alteration is invalid, the offending characters are ignored and a
 * warning is printed to stderr. Whitespace in the string is ignored between
 * alterations.
 */
face mkface(string);

/**
 * Like mkface(), but uses the given face as a starting point instead of 0.
 */
face mkface_of(face, string);

/**
 * Applies the given face to the given character, returning the transformed
 * character.
 */
qchar apply_face(face, qchar);
/**
 * Applies the given face to all members of the given mutable string in-place.
 */
void apply_face_str(face, mqstring);

#endif /* FACE_H_ */
