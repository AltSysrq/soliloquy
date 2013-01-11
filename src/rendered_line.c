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
    $q_Rendered_Line_meta is NULL, $llo_Rendered_Line_meta is consulted to
    generate $q_Rendered_Line_meta.

  SYMBOL: $q_Rendered_Line_body
    The formatted text of the line itself.

  SYMBOL: $q_Rendered_Line_meta
    The metadata for the line, in formatted text format.

  SYMBOL: $llo_Rendered_Line_meta
    How this will work is yet to be determined.
*/
defun($h_Rendered_Line) {
  //Eventually, we'll want to test $q_Rendered_Line_meta and generate it from
  //$llo_Rendered_Line_meta if it is NULL.
}
