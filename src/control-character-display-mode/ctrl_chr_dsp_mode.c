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
#include "ctrl_chr_dsp_mode.slc"
#include "face.h"

/*
  TITLE: Control Character Display Mode
  OVERVIEW: Adjusts the contents of BufferEditor and LineEditor lines to
    properly handle control characters. This includes expansion of horizontal
    tabulators. This is a global mode.
 */

/*
  SYMBOL: $u_control_character_display_mode
    Identifies hooks used by Control Character Display Mode

  SYMBOL: $y_Activity_control_character_display_mode
  SYMBOL: $y_Activity_control_character_display_mode_default
    Whether control character display mode is active for this Activity.
 */
STATIC_INIT_TO($y_Activity_control_character_display_mode_default, true)
defmode($c_Activity, $u_control_character_display_mode,
        $y_Activity_control_character_display_mode,
        $y_Activity_control_character_display_mode_default)

/*
  SYMBOL: $I_Activity_leading_tabulator_width
    The width of tabulator characters which occur at the beginning of a line.

  SYMBOL: $I_Activity_middle_tabulator_width
    The width (alignment) of tabulator characters which occur in locations
    other than the leading positions.

  SYMBOL: $I_Activity_control_character_face
    Face to use for miscelaneous control characters (ie, those displayed as
    '^X').

  SYMBOL: $I_Activity_tabulator_face
    The face to apply to visual indications of tabulators.

  SYMBOL: $x_Activity_tabulator_char
    Character to show to display tabulator characters. The default varies based
    on whether ADD_WCH_IS_BROKEN; if it is defined, it is a backquote;
    otherwise, it is a right-pointing guillemotte. When redefining, keep in
    mind whether your ncurses implementation can actually display non-ASCII
    characters.

  SYMBOL: $I_Activity_form_feed_face
    Face to apply to visualised form-feed characters.

  SYMBOL: $x_Activity_form_feed_char
    Lines which consist only of a form feed character are replaced by this
    character repeated $i_column_width times. The default uses the Unicode
    horizontal line character if ADD_WCH_IS_BROKEN is undefined; otherwise, it
    is a tilde.    
 */
ATSINIT {
  $y_Activity_control_character_display_mode_default = true;
  $I_Activity_leading_tabulator_width = 8;
  $I_Activity_middle_tabulator_width = 8;
  $I_Activity_control_character_face = mkface("!fr!U");
  $I_Activity_tabulator_face = mkface("*fK");
#ifndef ADD_WCH_IS_BROKEN
  $x_Activity_tabulator_char = L'»';
#else
  $x_Activity_tabulator_char = L'`';
#endif
  $I_Activity_form_feed_face = mkface("!fL");
#ifndef ADD_WCH_IS_BROKEN
  $x_Activity_form_feed_char = L'─';
#else
  $x_Activity_form_feed_char = L'~';
#endif
}

/*
  SYMBOL: $u_character_substitution
    Class for hooks which perform character substitution on the input string,
    possibly changing the length of the string in the process.
 */
mode_adv($u_character_substitution, $h_line_format_check) {
  // First, check for singular FF
  if (($q_line_format[0] & QC_CHAR) == L'\f' && $q_line_format[1] == 0) {
    $I_line_format_size += $i_column_width;
    $y_line_format_change = true;
    return;
  }

  // Look for character expansions. Since we can't rely on tabulators being
  // where they are by the time move() is called, we must assume thay are as
  // big as possible.
  unsigned tabsz = $I_Activity_leading_tabulator_width;
  if ($I_Activity_middle_tabulator_width > tabsz)
    tabsz = $I_Activity_middle_tabulator_width;
  unsigned ccsz = 2;

  for (qstring q = $q_line_format; *q; ++q) {
    qchar qch = *q & QC_CHAR;
    if (qch == L'\t')
      $I_line_format_size += tabsz-1;
    else if (qch < L' ' || qch == 127)
      $I_line_format_size += ccsz-1;

    if (qch < L' ' || qch == 127) {
      $y_line_format_change = true;
      $y_line_format_needs_back_buffer = true;
    }
  }
}

mode_adv($u_character_substitution, $h_line_format_move) {
  if (($Q_line_format[0] & QC_CHAR) == L'\f' && !$Q_line_format[1]) {
    // Fill with the FF character
    $Q_line_format[$i_column_width+1] = 0;
    qchar ffc = apply_face($I_Activity_form_feed_face,
                           $x_Activity_form_feed_char);
    qmemset($Q_line_format, ffc, $i_column_width);
    return;
  }

  // If we didn't request a back buffer, there are no control characters we
  // want to replace, and the loop below won't work.
  if (!$y_line_format_needs_back_buffer)
    return;

  // Swap the front and back buffers
  SWAP($Q_line_format, $Q_line_format_back);

  bool is_init_tab = true;
  unsigned dst_ix = 0;
  for (unsigned src_ix = 0; $Q_line_format_back[src_ix]; ++src_ix) {
    qchar qch = $Q_line_format_back[src_ix] & QC_CHAR;
    is_init_tab &= (qch == L'\t');

    if (qch >= L' ' && qch != 127) {
      // Not a control character, just copy
      $Q_line_format[dst_ix++] = $Q_line_format_back[src_ix];
    } else if (qch == L'\t') {
      unsigned tabsz;
      if (is_init_tab)
        tabsz = $I_Activity_leading_tabulator_width;
      else
        tabsz = $I_Activity_middle_tabulator_width;

      unsigned tgt = (dst_ix/tabsz + 1) * tabsz;
      // First character (the tabulation indicator)
      $Q_line_format[dst_ix++] = apply_face($I_Activity_tabulator_face,
                                            $x_Activity_tabulator_char);
      // Padding spaces
      while (dst_ix != tgt)
        $Q_line_format[dst_ix++] = L' ';
    } else {
      // Generic control character
      qchar str[2] = { L'^', L'X' };
      if (qch < L' ')
        str[1] = qch + L'@';
      else //qch==127
        str[1] = L'?';

      // Copy face of old control character over
      qchar attr = $Q_line_format_back[src_ix] & ~QC_CHAR;
      // Apply faces and write to destination
      for (unsigned i = 0; i < 2; ++i)
        $Q_line_format[dst_ix++] =
          apply_face($I_Activity_control_character_face, str[i] | attr);
    }
  }

  // Trailing zero
  $Q_line_format[dst_ix] = 0;
}
