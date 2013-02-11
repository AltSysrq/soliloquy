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

  SYMBOL: $c_RenderedLine
    Encapsulates a line of text already prepared to be displayed to the
    screen. A fully-constructed RenderedLine has its $q_RenderedLine_meta and
    $q_RenderedLine_body symbols set. When constructed, if
    $q_RenderedLine_meta is NULL, $m_gen_meta() is called to
    generate it.

  SYMBOL: $q_RenderedLine_body
    The formatted text of the line itself.

  SYMBOL: $q_RenderedLine_meta
    The metadata for the line, in formatted text format.

  SYMBOL: $f_RenderedLine_gen_meta
    Method on RenderedLine. Called from $f_RenderedLine() if
    $q_RenderedLine_meta was NULL. When this is called, $q_RenderedLine_meta
    has been initialised to a qchar array of length (1 + $i_line_meta_width),
    filled with NULs. Hooks to this point should populate at most
    $i_line_meta_width of characters, and should not alter characters set to
    non-NUL values by hooks that run before them (though changing formatting is
    acceptable).
    --
    Subclasses generally should avoid overriding this function unless they also
    call it.
 */
defun($h_RenderedLine) {
  if (!$q_RenderedLine_meta) {
    $q_RenderedLine_meta = qcalloc(1 + $i_line_meta_width);
    $m_gen_meta();
  }
}

/*
  SYMBOL: $f_RenderedLine_cvt $q_RenderedLine_cvt
    Concatenates the meta and body of this RenderedLine into
    $q_RenderedLine_cvt.
 */
defun($h_RenderedLine_cvt) {
  mqstring meta = qcalloc(1 + $i_line_meta_width);
  size_t max = qstrlen($q_RenderedLine_meta);

  for (unsigned i = 0; i < $i_line_meta_width; ++i)
    meta[i] = (i < max? ($q_RenderedLine_meta[i] ?: L' ') : L' ');
  $q_RenderedLine_cvt = qstrap(meta, $q_RenderedLine_body);
}

