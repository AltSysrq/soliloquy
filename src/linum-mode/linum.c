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
#include "linum.slc"
#include "face.h"

/*
  TITLE: Line-Number Mode
  OVERVIEW: Adds line numbers to BufferEditor line meta.
 */

/*
  SYMBOL: $u_line_number_mode $y_BufferEditor_line_number_mode
  SYMBOL: $y_BufferEditor_line_number_mode_default
    When active, adds line numbering to the line meta area.

  SYMBOL: $u_line_numbering
    Hook class for hooks which determine or affect line numbering.
 */
defmode($c_BufferEditor, $u_line_number_mode,
        $y_BufferEditor_line_number_mode,
        $y_BufferEditor_line_number_mode_default)

/*
  SYMBOL: $w_BufferEditor_line_number_rel
    String of characters to display to indicate relative line numbers. May be
    any length.

  SYMBOL: $w_BufferEditor_line_number_digits
    Digits to display for absolute line numbers. Must be exactly ten characters
    long.

  SYMBOL: $y_BufferEditor_line_number_mode_show_relative
    If true, relative line numbers will be shown for lines sufficiently close
    to point. This sacrifices one column of meta area.

  SYMBOL: $I_BufferEditor_line_number_here_face
    Face for a relative line number of zero.

  SYMBOL: $I_BufferEditor_line_number_pos_face
    Face for a positive relative line number.

  SYMBOL: $I_BufferEditor_line_number_neg_face
    Face for a negative relative line number.

  SYMBOL: $I_BufferEditor_line_number_face
    Face for absolute line numbers.
 */
STATIC_INIT_TO($y_BufferEditor_line_number_mode_default, true)
ATSINIT {
  $w_BufferEditor_line_number_rel =
    L"@123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  $w_BufferEditor_line_number_digits = L"0123456789";

  $y_BufferEditor_line_number_mode_show_relative = true;
  $I_BufferEditor_line_number_here_face = mkface("+X");
  $I_BufferEditor_line_number_pos_face = mkface("!fm");
  $I_BufferEditor_line_number_neg_face = mkface("!fc");
  $I_BufferEditor_line_number_face = mkface("!fb");
}

mode_adv_after($u_line_numbering, $h_RenderedLine_gen_meta) {
  // Do nothing if not in the right context
  if (!$o_BufferEditor_buffer) return;
  dynar_w contents = $($o_BufferEditor_buffer, $aw_FileBuffer_contents);
  if (!contents) return;

  // Count how many characters are available
  unsigned avail = 0;
  for (int i = 0; i < $i_line_meta_width; ++i)
    if (!$q_RenderedLine_meta[i])
      ++avail;

  if (!avail) return;

  qchar chars[avail];
  memset(chars, 0, sizeof(chars));

  // Handle relative line numbers
  if ($y_BufferEditor_line_number_mode_show_relative && avail >= 1) {
    int rel =
      $($o_BufferEditor_point, $I_FileBufferCursor_line_number) -
      (signed)$I_BufferEditor_index;

    face rel_face = $I_BufferEditor_line_number_pos_face;
    if (rel < 0) {
      rel *= -1;
      rel_face = $I_BufferEditor_line_number_neg_face;
    } else if (rel == 0) {
      rel_face = $I_BufferEditor_line_number_here_face;
    }

    if (rel < wcslen($w_BufferEditor_line_number_rel))
      chars[avail-1] = apply_face(rel_face,
                                  $w_BufferEditor_line_number_rel[rel]);

    --avail;
  }

  if (avail > 0) {
    // See how many digits we need to display absolute line numbers
    unsigned num_digits = 0, max = contents->len;
    while (max)
      ++num_digits, max /= 10;

    if (!num_digits)
      num_digits = 1;

    unsigned num;
    if (avail >= num_digits) {
      // The whole absolute number can fit
      num = $I_BufferEditor_index+1;
    } else {
      // Only some of the lower digits can fit; reserve the uppermost digit for
      // a rotating display of the digits which don't fit
      unsigned mod = 1;
      for (unsigned i = 0; i < avail-1; ++i)
        mod *= 10;

      num = ($I_BufferEditor_index+1) % mod;
    }
    
    for (unsigned ix = avail-1; num > 0; --ix, num /= 10)
      chars[ix] = apply_face($I_BufferEditor_line_number_face,
                             $w_BufferEditor_line_number_digits[num%10]);

    // TODO: rotating display of upper digits
  }

  // Write back to the meta
  mqstring new = qcalloc($i_line_meta_width+1);
  for (unsigned i = 0, j = 0; i < (unsigned)$i_line_meta_width; ++i) {
    if ($q_RenderedLine_meta[i])
      new[i] = $q_RenderedLine_meta[i];
    else
      new[i] = chars[j++];
  }
  $q_RenderedLine_meta = new;
}
