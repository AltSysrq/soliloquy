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
#include "transcript.slc"

/*
  TITLE: Transcript Workspace Backing
  OVERVIEW: The Transcript is the primary Backing type in Soliloquy. It is
    essentially an output log, like what terminal emulators provide, but
    highlights output groups and allows to easily reference output groups (eg,
    to turn them into pins).
 */

/*
  SYMBOL: $c_Transcript
    The Transcript is the primary Backing type in Soliloquy. It is essentially
    an output log, like what terminal emulators provide, but highlights output
    groups and allows to easily reference output groups (eg, to turn them into
    pins).
 */
subclass($c_Backing, $c_Transcript)

STATIC_INIT_TO($i_Transcript_num_output_groups, 32)

/*
  SYMBOL: $ai_Transcript_output_groups
    An array of even length. Each even index is the line index of the start of
    an output group (the top of it), and each corresponding odd index is the
    length of that output group.
    Blocks which "start" at line -1 do not exist.

  SYMBOL: $i_Transcript_num_output_groups
    The initial number of groups in $ai_Transcript_output_groups (ie, half its
    length). This is only used to construct $ai_Transcript_output_groups, and
    is unused afterward.

  SYMBOL: $ai_Transcript_line_refs
    An array of indices into $ao_Backing_lines which must be maintained. These
    are used to maintain references into the lines array, even in the presence
    of structural changes to the array. Entries which are -1 indicate deleted
    references. The zeroth element should never be -1.

  SYMBOL: $i_Transcript_line_ref_offset
    The logical index of the zeroth element of $ai_Transcript_line_refs.

  SYMBOL: $y_Transcript_next_group_colour
    Toggled every time an output group is appended. Used to colour
    Rendered_Line metadata to allow to easily distinguish output groups.
 */
defun($h_Transcript) {
  $i_Transcript_line_ref_offset = 0;
  $ai_Transcript_output_groups = dynar_new_i();
  dynar_expand_by_i($ai_Transcript_output_groups,
                    2*$i_Transcript_num_output_groups);
  memset($ai_Transcript_output_groups->v, -1,
         sizeof(int)*$ai_Transcript_output_groups->len);

  $ai_Transcript_line_refs = dynar_new_i();
}
