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

  SYMBOL: $s_FileBuffer_filename
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
