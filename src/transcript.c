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
#include "face.h"

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
  SYMBOL: $i_Transcript_max_size
    Whenever the size of a Transcript exceeds this amount in lines, the first
    $i_Transcript_truncation_amt lines will be deleted, provided doing so would
    not invalidate any line references.

  SYMBOL: $i_Transcript_truncation_amt
    The number of lines to delete from the head of a Transcript when its length
    exceeds $i_Transcript_max_size.
 */
STATIC_INIT_TO($i_Transcript_max_size, 65536)
STATIC_INIT_TO($i_Transcript_truncation_amt, 1024)

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
    RenderedLine metadata to allow to easily distinguish output groups.
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

/*
  SYMBOL: $f_Transcript_append
    Append lines to the end of the transcript, which do not form output
    groups.

  SYMBOL: $lo_Transcript_output
    A list of RenderedLine objects. The lines to append to the Transcript, in
    a call to $f_Transcript_append or $f_Transcript_group. Will be NULLed after
    those calls.
 */
defun($h_Transcript_append) {
  $M_alter(0,0,
           $i_Backing_alteration_begin =
             $ao_Backing_lines->len,
           $i_Backing_ndeletions = 0,
           $lo_Backing_replacements =
             $lo_Transcript_output);
  $lo_Transcript_output = NULL;

  $m_check_size();
}

/*
  SYMBOL: $I_Transcript_even_group_meta_highlight_face
    The face to apply to the line metadata of every line of even output groups
    in Transcripts. This should be a highlighting (ie, uniform colour
    alteration).

  SYMBOL: $I_Transcript_odd_group_meta_highlight_face
    The face to apply to the line metadata of every line of odd output groups
    in Transcripts. This should be a highlighting (ie, uniform colour
    alteration).
 */
STATIC_INIT_TO($I_Transcript_even_group_meta_highlight_face,
               mkface("!fb!bb"))
STATIC_INIT_TO($I_Transcript_odd_group_meta_highlight_face,
               mkface("!fr!br"));
/*
  SYMBOL: $f_Transcript_group
    Like $f_Transcript_append, but handles the text as a group. Highlighting
    will be applied to the metadata (by cloning the RenderedLines before
    appending), and a reference to the group added to
    $ai_Transcript_output_groups.
 */
defun($h_Transcript_group) {
  face group_face;
  if ($y_Transcript_next_group_colour) {
    group_face = $I_Transcript_even_group_meta_highlight_face;
  } else {
    group_face = $I_Transcript_odd_group_meta_highlight_face;
  }
  $y_Transcript_next_group_colour = !$y_Transcript_next_group_colour;

  // Apply face to all metadata
  $lo_Transcript_output =
    map_o($lo_Transcript_output,
          lambda((object o), ({
                mqstring new_meta = qcalloc($i_line_meta_width+1);
                qmemcpy(new_meta, $(o, $q_RenderedLine_meta), $i_line_meta_width);
                apply_face_arr(group_face, new_meta, $i_line_meta_width);
                $c_RenderedLine($q_RenderedLine_body =
                                   $(o, $q_RenderedLine_body),
                                 $q_RenderedLine_meta = new_meta);
              })));

  // Add this group to the group array
  int begin = $ao_Backing_lines->len;
  int len = llen_o($lo_Transcript_output);
  memmove($ai_Transcript_output_groups->v+2,
          $ai_Transcript_output_groups->v+0,
          sizeof(int)*($ai_Transcript_output_groups->len-2));
  $ai_Transcript_output_groups->v[0] = begin;
  $ai_Transcript_output_groups->v[1] = len;

  // Append the actual text
  $M_append(0,0);
}

/*
  SYMBOL: $f_Transcript_add_ref_line
    Appends a line to the transcript which will be alterable after being
    added. $o_Transcript_ref_line is the line of text to the
    Transcript. $i_Transcript_line_ref will be set to the reference of that
    line which can be accessed later.

  SYMBOL: $o_Transcript_ref_line
    The text to associate with an existing or new mutable line reference.

  SYMBOL: $i_Transcript_line_ref
    An integer reference to a mutable line.
 */
defun($h_Transcript_add_ref_line) {
  $i_Transcript_line_ref =
    $i_Transcript_line_ref_offset +
      $ai_Transcript_line_refs->len;
  dynar_push_i($ai_Transcript_line_refs, $i_Transcript_line_ref);
  $M_append(0,0,
            $lo_Transcript_output =
              cons_o($o_Transcript_ref_line, NULL));
}

/*
  SYMBOL: $f_Transcript_change_ref_line
    Alters the mutable line referenced by $i_Transcript_line_ref to hold the
    text $o_Transcript_ref_line. (Note that there is no way to delete the line
    or make it multiple lines, by design.)
 */
defun($h_Transcript_change_ref_line) {
  if ($i_Transcript_line_ref < $i_Transcript_line_ref_offset ||
      $i_Transcript_line_ref >=
      $i_Transcript_line_ref_offset + $ai_Transcript_line_refs->len ||
      -1 == $ai_Transcript_line_refs->v[$i_Transcript_line_ref -
                                        $i_Transcript_line_ref_offset])
    // Non-existent
    return;

  $M_alter(0,0,
           $i_Backing_alteration_begin =
             $ai_Transcript_line_refs->v[$i_Transcript_line_ref -
                                         $i_Transcript_line_ref_offset],
           $lo_Backing_replacements =
             cons_o($o_Transcript_ref_line, NULL),
           $i_Backing_ndeletions = 1);
}

/*
  SYMBOL: $f_Transcript_release_ref_line
    Invalidates the mutable line reference indicated by $i_Transcript_line_ref.
    This does not delete the line; it simply causes the reference to no longer
    apply.
 */
defun($h_Transcript_release_ref_line) {
  if ($i_Transcript_line_ref < $i_Transcript_line_ref_offset ||
      $i_Transcript_line_ref >=
      $i_Transcript_line_ref_offset + $ai_Transcript_line_refs->len ||
      -1 == $ai_Transcript_line_refs->v[$i_Transcript_line_ref -
                                        $i_Transcript_line_ref_offset])
    // Non-existent
    return;

  $ai_Transcript_line_refs->v[$i_Transcript_line_ref -
                              $i_Transcript_line_ref_offset] = -1;

  //Trim array
  unsigned off = 0;
  while ($ai_Transcript_line_refs->v[off] == -1)
    ++off;

  if (off) {
    memmove($ai_Transcript_line_refs->v,
            $ai_Transcript_line_refs->v+off,
            sizeof(int)*($ai_Transcript_line_refs->len-off));
    dynar_contract_by_i($ai_Transcript_line_refs, off);
    $i_Transcript_line_ref_offset += off;
  }

  off = 0;
  while ($ai_Transcript_line_refs->v[$ai_Transcript_line_refs->len - off - 1])
    ++off;
  if (off)
    dynar_contract_by_i($ai_Transcript_line_refs, off);
}

/*
  SYMBOL: $f_Transcript_check_size
    Examines the size of the Transcript, and truncates the head if it is too
    large, maintaining line references as needed.
 */
defun($h_Transcript_check_size) {
  // TODO
}
