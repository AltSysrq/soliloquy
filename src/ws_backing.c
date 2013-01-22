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
#include "ws_backing.slc"

/*
  TITLE: Workspace Backing Object
  OVERVIEW: A Workspace Backing stores an array of RenderedLines, and supports
    notifications of when these change.
*/

/*
  SYMBOL: $c_Backing
    A Workspace Backing object. The base class by itself does not do much of
    interest, other than manage $ao_Backing_lines.

  SYMBOL: $ao_Backing_lines
    The array of RenderedLines stored in this Backing. Third parties,
    including subclasses, should consider this read-only; the Backing
    manipulation functions should be used with $f_Backing_alter.
*/

defun($h_Backing) {
  $ao_Backing_lines = dynar_new_o();
}

/*
  SYMBOL: $f_Backing_alter
    Performs structural modifications of the Backing, which consist of zero or
    more insertions and zero or more deletions beginning at the line indexed by
    $i_Backing_alteration_begin. $i_Backing_ndeletions indicates how many lines
    to delete; $lo_Backing_replacements is a list of lines to insert into the
    array, which will be inserted in the order they exist in the list. Combined
    insertions and deletions have the effect of inserting the requested lines,
    then deleting lines *after* the insertion. In other words, a combined
    insertion of N lines and deletion of N lines will replace those N
    lines. Hooking into the MAIN part of this function is not useful, since it
    destroys some of its input parameters during operation.

  SYMBOL: $y_Backing_alteration_was_append
    Set by $f_Backing_alter to indicate whether the operations it performed
    were strictly appends; ie, whether the only change was that new lines were
    added to the end of the Backing, and all previous contents unchanged.

  SYMBOL: $i_Backing_alteration_begin
    The index of the first line to delete or before which to insert when
    calling $f_Backing_alter.

  SYMBOL: $i_Backing_ndeletions
    The number of deletions to perform in $f_Backing_alter. This is destroyed
    by calling $f_Backing_alter.

  SYMBOL: $lo_Backing_replacements
    The lines to insert when calling $f_Backing_alter. This is destroyed during
    that call.
 */
defun($h_Backing_alter) {
  $y_Backing_alteration_was_append = true;

  //First, in-place replacements
  unsigned ix = $i_Backing_alteration_begin;
  while ($lo_Backing_replacements && $i_Backing_ndeletions) {
    $ao_Backing_lines->v[ix++] = $lo_Backing_replacements->car;
    $lo_Backing_replacements = $lo_Backing_replacements->cdr;
    --$i_Backing_ndeletions;

    $y_Backing_alteration_was_append = false;
  }

  //Trailing deletes
  if ($i_Backing_ndeletions) {
    memmove($ao_Backing_lines->v + ix,
            $ao_Backing_lines->v + ix + $i_Backing_ndeletions,
            sizeof(object) *
              ($ao_Backing_lines->len - ix - $i_Backing_ndeletions));
    dynar_contract_by_o($ao_Backing_lines, $i_Backing_ndeletions);
    $y_Backing_alteration_was_append = false;
  } else if ($lo_Backing_replacements) {
    //Insertions
    size_t cnt = llen_o($lo_Backing_replacements);
    dynar_expand_by_o($ao_Backing_lines, cnt);
    //Handle the case of appending specially, since it's quite common and much
    //faster.
    if (ix+cnt == $ao_Backing_lines->len) {
      while ($lo_Backing_replacements) {
        $ao_Backing_lines->v[ix++] = $lo_Backing_replacements->car;
        $lo_Backing_replacements = $lo_Backing_replacements->cdr;
      }
    } else {
      //Middle-of-array insertions
      $y_Backing_alteration_was_append = false;
      memmove($ao_Backing_lines->v + ix + cnt,
              $ao_Backing_lines->v + ix,
              ($ao_Backing_lines->len - ix - cnt)*sizeof(object));
      while ($lo_Backing_replacements) {
        $ao_Backing_lines->v[ix++] = $lo_Backing_replacements->car;
        $lo_Backing_replacements = $lo_Backing_replacements->cdr;
      }
    }
  }
}
