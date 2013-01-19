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
#include "rendered_line.slc"

/*
  TITLE: Rendered Line
  OVERVIEW: Encapsulates a line of text already prepared to be displayed to the
    screen, complete with metadata.

  SYMBOL: $c_Rendered_Line
    Encapsulates a line of text already prepared to be displayed to the
    screen. A fully-constructed Rendered_Line has its $q_Rendered_Line_meta and
    $q_Rendered_Line_body symbols set. When constructed, if
    $q_Rendered_Line_meta is NULL, $m_gen_meta() is called to
    generate it.

  SYMBOL: $q_Rendered_Line_body
    The formatted text of the line itself.

  SYMBOL: $q_Rendered_Line_meta
    The metadata for the line, in formatted text format.

  SYMBOL: $H_gen_meta
    Method on Rendered_Line. Called from $f_Rendered_Line() if
    $q_Rendered_Line_meta was NULL. When this is called, $q_Rendered_Line_meta
    has been initialised to a qchar array of length (1 + $i_line_meta_width),
    filled with NULs. Hooks to this point should populate at most
    $i_line_meta_width of characters, and should not alter characters set to
    non-NUL values by hooks that run before them (though changing formatting is
    acceptable).
 */
defun($h_Rendered_Line) {
  if (!$q_Rendered_Line_meta) {
    $q_Rendered_Line_meta = gcalloc(sizeof(qchar)*(1 + $i_line_meta_width));
    $m_gen_meta();
  }
}
