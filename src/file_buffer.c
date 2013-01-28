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
#include "file_buffer.slc"
#include <errno.h>
#include <time.h>

/*
  TITLE: (Textual) File Buffer
  OVERVIEW: Efficiently manages the lines of text in a file, including undo
    states.

  NOTES: Shared Undo Log Format
    Undo events are stored in a semi-human-readable, line-based log file. The
    first line is ignored, and typically holds identifying information. The
    rest of the file consists of undo records.

    An undo record starts with a line beginning with the character '@', and
    runs until but not including the next such line, or the end of the file,
    whichever comes first. Such lines have the format
      @%X,%X,%X:%s
    which records the byte offset of the previous undo record (or 0 for none),
    the 0-based line number of the edit, the timestamp of the edit, and the
    name of the file this applies to (or the empty string if it is the same as
    the physically preceding record).

    Following the undo record header are zero or more edit records, one per
    line. Each edit record begins with either a '+' (insertion) or '-'
    (deletion), and is followed by the text of the line affected. Edits are
    always written from the perspective of performing them; the contents of the
    lines in both directions are recorded to allow both undo and redo to
    operate.

    A line which looks like an undo record, except that it begins with an '&',
    specifies a sub-undo record. When a sub-undo record is to be undone, the
    previous undo record it points to must also be undone. Similarly, when
    re-doing an undo record, any following sub-undo records must be
    re-done. Sub-undo records allow for efficiently recording sparse, possibly
    non-sorted edits to the file as a single unit.
 */

/*
  SYMBOL: $c_FileBufferCursor
    Maintains a reference to a location within a FileBuffer, automatically
    being updated as the buffer is changed, so that the same logical line is
    referred to. $o_FileBufferCursor_buffer must be set by the caller of the
    constructor. $I_FileBufferCursor_line_number may be set by the caller, but
    defaults to 0 (the first line).

  SYMBOL: $o_FileBufferCursor_buffer
    The buffer this cursor referrs to.

  SYMBOL: $I_FileBufferCursor_line_number
    The current 0-based line number this cursor refers to. This ranges from
    zero to the length of the file in lines, inclusive.

  SYMBOL: $I_FileBufferCursor_window
    If non-zero, defines a window size for this cursor. Any edits which occur
    between $I_FileBufferCursor_line_number, inclusive, and
    $I_FileBufferCursor_line_number+$I_FileBufferCursor_window, exclusive, will
    cause $m_window_changed() to be called on the cursor.
 */
defun($h_FileBufferCursor) {
  $$($o_FileBufferCursor_buffer) {
    lpush_o($lo_FileBuffer_cursors, $o_FileBufferCursor);
  }
}

/*
  SYMBOL: $f_FileBufferCursor_destroy
    De-registers the FileBufferCursor from its associated FileBuffer.
 */
defun($h_FileBufferCursor_destroy) {
  $$($o_FileBufferCursor_buffer) {
    $lo_FileBuffer_cursors = lrm_o($lo_FileBuffer_cursors,
                                   $o_FileBufferCursor);
  }
}

/*
  SYMBOL: $f_FileBufferCursor_shunt
    Called when the FileBufferCursor must be moved due to insertions or
    deletions within the FileBuffer. The line number shall be altered by an
    amount specified in $i_FileBufferCursor_shunt_distance.

  SYMBOL: $i_FileBufferCursor_shunt_distance
    The line number delta to use in $f_FileBufferCursor_shunt.
 */
defun($h_FileBufferCursor_shunt) {
  $I_FileBufferCursor_line_number += $i_FileBufferCursor_shunt_distance;
}

/*
  SYMBOL: $f_FileBufferCursor_window_changed
    Called when the cursor has a notification window (see
    $I_FileBufferCursor_window) and the region defined thereby was changed. The
    default does nothing; it only exists so that there is something to hook
    onto with the base FileBufferCursor class.
 */
defun($h_FileBufferCursor_window_changed) {}

/*
  SYMBOL: $c_FileBuffer
    Manages a single file- or memory-backed, editable buffer. Memory-backed
    buffers always have their contents stored in memory, but only
    recently-accessed file-backed buffers keep their contents in memory. If an
    external operation fails, it rolls the current transaction back.

  SYMBOL: $p_shared_undo_log
    A FILE* where all undo information for all FileBuffers is kept. The file
    will be deleted when the program exits. It is opened the first time
    $c_FileBuffer is called. Its format is described after the OVERVIEW section
    in the src/file_buffer.c documentation.

  SYMBOL: $w_FileBuffer_filename
    The name (absolute path) of the file being operated on by the
    FileBuffer. For memory-backed FileBuffers, it is not necessarily a
    filename.

  SYMBOL: $y_FileBuffer_memory_backed
    If true, this FileBuffer is backed by memory instead of a file.

  SYMBOL: $aw_FileBuffer_contents
    The contents of this FileBuffer. This may be NULL, indicating that the
    contents currently reside on disk. Any operation that needs this value
    should call $f_FileBuffer_access() to ensure that it is non-NULL and to
    update the access time.

  SYMBOL: $ao_FileBuffer_meta
    Arbitrary data to associate with each line. This array is transient; it is
    released whenever $aw_FileBuffer_contents is. When re-loaded, its objects
    are empty.

  SYMBOL: $lo_FileBuffer_cursors
    A list of all FileBufferCursors currently associated with this FileBuffer.
 */
defun($h_FileBuffer) {
  if (!$p_shared_undo_log) {
    //$p_shared_undo_log = tmpfile();
    $p_shared_undo_log = fopen("sol.out", "w+b");
    if (!$p_shared_undo_log)
      tx_rollback_errno($u_FileBuffer);

    if (-1 == fputws(L"Soliloquy Undo Journal\n", $p_shared_undo_log)) {
      int old_errno = errno;
      fclose($p_shared_undo_log);
      $p_shared_undo_log = NULL;

      errno = old_errno;
      tx_rollback_errno($u_FileBuffer);
    }
  }
}

/*
  SYMBOL: $f_FileBuffer_access
    Ensures that $aw_FileBuffer_contents is loaded, and checks for changes to
    the source file.
 */
defun($h_FileBuffer_access) {
  if (!$aw_FileBuffer_contents)
    $m_reload();

  //TODO: Maintain access time; check for changes in source file
}

/*
  SYMBOL: $f_FileBuffer_reload
    Reloads this FileBuffer, so that $aw_FileBuffer_contents and
    $ao_FileBuffer_meta are non-NULL. This should not be called directly; use
    $f_FileBuffer_access() instead. It is provided as a separate function for
    hooking purposes only.
 */
defun($h_FileBuffer_reload) {
  if (!$aw_FileBuffer_contents) {
    if ($y_FileBuffer_memory_backed) {
      $aw_FileBuffer_contents = dynar_new_w();
    } else {
      $v_rollback_type = $u_FileBuffer;
      $s_rollback_reason = "File-backed FileBuffer not yet supported";
      tx_rollback();
    }
  }

  if (!$ao_FileBuffer_meta) {
    $ao_FileBuffer_meta = dynar_new_o();
    dynar_expand_by_o($ao_FileBuffer_meta, $aw_FileBuffer_contents->len);
    for (unsigned i = 0; i < $aw_FileBuffer_contents->len; ++i)
      $ao_FileBuffer_meta->v[i] = object_new(NULL);
  }
}

/*
  SYMBOL: $f_FileBuffer_release
    Releases $ao_FileBuffer_meta and $aw_FileBuffer_contents for this
    FileBuffer.
 */
defun($h_FileBuffer_release) {
  $ao_FileBuffer_meta = NULL;
  $aw_FileBuffer_contents = NULL;
}

STATIC_INIT_TO($w_prev_undo_name, L"")
/*
  SYMBOL: $f_FileBuffer_edit
    Edits the buffer by performing $I_FileBuffer_ndeletions deletions starting
    at $I_FileBuffer_edit_line, then inserting the contents of
    $lw_FileBuffer_replacements before the first line unaffected by
    deletion. Inserted and replaced lines have their meta objects reset to
    empty objects. $y_FileBuffer_continue_undo will be set to false after this
    call. $f_FileBuffer_access will be called automatically.

  SYMBOL: $I_FileBuffer_edit_line
    The line at which edits occur for this FileBuffer.

  SYMBOL: $y_FileBuffer_continue_undo
    If true when $f_FileBuffer_edit is called, the new undo record will instead
    be a sub-record of the previous record. This is set to false after each
    call to $f_FileBuffer_edit.

  SYMBOL: $I_FileBuffer_ndeletions
    The number of lines to delete in a call to $f_FileBuffer_edit.

  SYMBOL: $lw_FileBuffer_replacements
    The lines to insert in a call to $f_FileBuffer_edit.

  SYMBOL: $I_FileBuffer_undo_offset
    The offset of the most recent undo state for this FileBuffer, or 0 if there
    is no undo information.

  SYMBOL: $w_prev_undo_name
    The last filename written in an undo record header.
 */
defun($h_FileBuffer_edit) {
  $m_access();

  wchar_t undo_type =
    ($y_FileBuffer_continue_undo? L'&' : L'@');
  $y_FileBuffer_continue_undo = false;

  // Cap ndeletions if it would run past the end of the buffer
  if ($I_FileBuffer_edit_line + $I_FileBuffer_ndeletions >
      $az_LineEditor_buffer->len)
    $I_FileBuffer_ndeletions =
      $az_LineEditor_buffer->len - $I_FileBuffer_edit_line;

  //Abort if out of range
  if ($I_FileBuffer_edit_line > $az_LineEditor_buffer->len) {
    $v_rollback_type = $u_FileBuffer;
    $s_rollback_reason = "$I_FileBuffer_edit_line out of range";
    tx_rollback();
  }

  unsigned ndeletions = $I_FileBuffer_ndeletions;
  unsigned ninsertions = llen_w($lw_FileBuffer_replacements);
  unsigned nreplacements =
    ndeletions > ninsertions? ninsertions : ndeletions;
  list_w insertions = $lw_FileBuffer_replacements;

  unsigned long long now = time(0);
  unsigned prev_undo = $I_FileBuffer_undo_offset;

  //Seek to the end of the file
  $I_FileBuffer_undo_offset = fseek($p_shared_undo_log, 0, SEEK_END);
  if (!~$I_FileBuffer_undo_offset)
    // Seek failed for some reason
    tx_rollback_errno($u_FileBuffer);

  //Write the header for this undo record
  if (-1 ==
      fwprintf($p_shared_undo_log, L"%lc%X,%X,%llX:%ls\n",
               undo_type,
               prev_undo,
               $I_FileBuffer_edit_line,
               now,
               wcscmp($w_prev_undo_name, $w_FileBuffer_filename)?
               $w_FileBuffer_filename : L""))
    tx_rollback_errno($u_FileBuffer);

  //Whether we like it or not, this is now unconditionally the new undo offset
  tx_write_through($I_FileBuffer_undo_offset);
  $w_prev_undo_name = $w_FileBuffer_filename;
  tx_write_through($w_prev_undo_name);

  //Write deletions
  for (unsigned i = 0; i < $I_FileBuffer_ndeletions; ++i)
    if (-1 == fwprintf($p_shared_undo_log, L"-%ls\n",
                       $aw_FileBuffer_contents->v[i + $I_FileBuffer_edit_line]))
      tx_rollback_errno($u_FileBuffer);

  //Write insertions
  for (list_w curr = insertions; curr; curr = curr->cdr)
    if (-1 == fwprintf($p_shared_undo_log, L"+%ls\n", curr->car))
      tx_rollback_errno($u_FileBuffer);

  //Make the changes
  for (unsigned i = 0;
       insertions && i < ndeletions;
       ++i, insertions = insertions->cdr) {
    unsigned line = i + $I_FileBuffer_edit_line;
    $aw_FileBuffer_contents->v[line] = insertions->car;
    $ao_FileBuffer_meta->v[line] = object_new(NULL);
  }

  if (insertions) {
    unsigned line = $I_FileBuffer_edit_line + ndeletions;
    unsigned cnt = ninsertions - ndeletions;
    wstring tail[cnt];
    object meta[cnt];
    unsigned ix = 0;
    each_w(insertions, lambdav((wstring s), tail[ix++] = s));
    for (unsigned i = 0; i < cnt; ++i)
      meta[i] = object_new(NULL);

    dynar_ins_w($aw_FileBuffer_contents, line, tail, cnt);
    dynar_ins_o($ao_FileBuffer_meta, line, meta, cnt);
  } else if (ndeletions > ninsertions) {
    unsigned line = $I_FileBuffer_edit_line + ninsertions;
    unsigned cnt = ndeletions - ninsertions;

    dynar_erase_w($aw_FileBuffer_contents, line, cnt);
    dynar_erase_o($ao_FileBuffer_meta, line, cnt);
  }

  // Update cursors as necessary
  for (list_o curr = $lo_FileBuffer_cursors; curr; curr = curr->cdr) {
    object cursor = curr->car;
    unsigned where = $(cursor, $I_FileBufferCursor_line_number);
    unsigned window = $(cursor, $I_FileBufferCursor_window);

    if (where >= $I_FileBuffer_edit_line + nreplacements) {
      if (ninsertions > ndeletions) {
        // Need to shunt downward by the number of new lines.
        $M_shunt(0, cursor,
                 $i_FileBufferCursor_shunt_distance =
                   +1 * (signed)(ninsertions - ndeletions));

      } else if (ndeletions > ninsertions) {
        // There are two cases here.
        // If the cursor was in the region that was deleted, it should just be
        // shunted to the first line not deleted. Otherwise, it should be
        // shunted back by the relative number of lines deleted.
        unsigned dist;
        if (where < $I_FileBuffer_edit_line + ndeletions)
          dist = where - $I_FileBuffer_edit_line;
        else
          dist = ndeletions - ninsertions;

        $M_shunt(0, cursor,
                 $i_FileBufferCursor_shunt_distance =
                   -1 * (signed)dist);
      }

      where = $(cursor, $I_FileBufferCursor_line_number);
      window = $(cursor, $I_FileBufferCursor_window);
    }

    /* If the cursor has a notification window, and that window overlaps any
     * area that has changed, we must notify the cursor. The region is from
     * where (inclusive) to where+window (exclusive). If window==0, there is no
     * region.
     */
    if (window) {
      unsigned region_begin = where;
      unsigned region_end = where+window;
      unsigned changed_begin = $I_FileBuffer_edit_line;
      unsigned changed_end = changed_begin + ninsertions + 1;
      if ((region_begin >= changed_begin && region_begin < changed_end) ||
          (region_end > changed_begin && region_end <= changed_end) ||
          (changed_begin >= region_begin && changed_begin < region_end) ||
          (changed_end > region_begin && changed_end <= region_end)) {
        $M_window_changed(0, cursor);
      }
    }
  }
}
