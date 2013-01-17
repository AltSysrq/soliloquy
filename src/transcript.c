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

defun($h_Transcript) {
  $ai_Transcript_output_groups = dynar_new_i();
  dynar_expand_by_i($ai_Transcript_output_groups,
                    2*$i_Transcript_num_output_groups);
  memset($ai_Transcript_output_groups->v, -1,
         sizeof(int)*$ai_Transcript_output_groups->len);
}
