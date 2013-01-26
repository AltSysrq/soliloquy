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
    states. All information regarding the file is stored in an append-only
    random-access file, allowing for efficient manipulation of undo/redo
    states, etc.
 */

/*
  SYMBOL: $c_FileBufferCursor
    Maintains a reference into a FileBuffer, which is used to read, write,
    insert, and delete lines.

  SYMBOL: $I_FileBufferCursor_line_number
    The current physical line number of this cursor within the buffer, where 1
    is the first line. 0 indicates an invalid cursor.

  SYMBOL: $I_FileBufferCursor_offset
    The physical data offset of this cursor within the buffer.

  SYMBOL: $o_FileBufferCursor_buffer
    The FileBuffer this cursor is a member of.

  SYMBOL: $v_FileBufferCursor_shunt_mode
    Determines how the cursor behaves when the line it references is
    deleted. This may be $u_shunt_upward, $u_shunt_downward, or $u_invalidate.

  SYMBOL: $u_shunt_upward
    When applied to $v_FileBufferCursor_shunt_mode, the cursor will move up to
    the previous remaining line when its parent is destroyed, if possible. If
    the top-most line of the file is also deleted, it will remain on the new
    line 1.

  SYMBOL: $u_shunt_downward
    When applied to $v_FileBufferCursor_shunt_mode, the cursor will move down
    to the next remaining line when its parent is destroyed, if possible. If
    all lines to the end of the file are deleted, it will remain pointing to
    the end of the file.

  SYMBOL: $u_invalidate
    When applied to $v_FileBufferCursor_shunt_mode, the cursor will become
    invalid if the line it referenced is deleted. The caller must validate the
    cursor before using it.
 */
defun($h_FileBufferCursor) {
  $I_FileBufferCursor_line_number = 1;
  $$($o_FileBufferCursor_buffer) {
    $I_FileBufferCursor_offset = $I_FileBuffer_root_offset;
    lpush_o($lo_FileBuffer_cursors, $o_FileBufferCursor);
  }

  if (!$v_FileBufferCursor_shunt_mode)
    $v_FileBufferCursor_shunt_mode = $u_shunt_upward;
}

/*
  SYMBOL: $f_FileBufferCursor_destroy
    Releases the link between this cursor and its buffer.
 */
defun($h_FileBufferCursor_destroy) {
  $$($o_FileBufferCursor_buffer) {
    $lo_FileBuffer_cursors = lrm_o($lo_FileBuffer_cursors, $o_FileBufferCursor);
  }
  $o_FileBufferCursor = NULL;
}

/*
  SYMBOL: $f_FileBufferCursor_rewind
    Resets this cursor to the beginning of the file. This is the only operation
    (other than destroy) which can be performed on an invalid cursor. In any
    case, the cursor will be valid after this call.
 */
defun($h_FileBufferCursor_rewind) {
  $$($o_FileBufferCursor) {
    $I_FileBufferCursor_line_number = 1;
    $I_FileBufferCursor_offset = $I_FileBuffer_root_offset;
  }
}

/*
  SYMBOL: $f_FileBufferCursor_eof $y_FileBufferCursor_eof
    Queries whether this cursor is currently at the end of the file, setting
    $y_FileBufferCursor_eof according to the result.
 */
defun($h_FileBufferCursor_eof) {
  $y_FileBufferCursor_eof =
    $M_next_offset(!!$I_FileBuffer_next_offset,
                   $o_FileBufferCursor_buffer);
}

/*
  SYMBOL: $f_FileBufferCursor_advance
    Moves this FileBufferCursor forward one line. The cursor becomes invalid if
    this moves it past the end of the file.
 */
defun($h_FileBufferCursor_advance) {
  if ($M_eof($y_FileBufferCursor_eof,0)) {
    $I_FileBufferCursor_line_number = 0;
    return;
  }

  ++$I_FileBufferCursor_line_number;
  $I_FileBufferCursor_offset =
    $M_links($I_FileBuffer_next_offset,
             $o_FileBufferCursor_buffer);
}

/*
  SYMBOL: $f_FileBufferCursor_sof $y_FileBufferCursor_sof
    Queries whether this cursor is at the start of the file, setting
    $y_FileBufferCursor_sof to indicate the result.
 */
defun($h_FileBufferCursor_sof) {
  $y_FileBufferCursor_sof =
    $M_links(!!$I_FileBuffer_prev_offset,
             $o_FileBufferCursor_buffer);
}

/*
  SYMBOL: $f_FileBufferCursor_retreat
    Moves this cursor backward in the buffer by one line. If the cursor was
    already at start of file, it becomes invalid.
 */
defun($h_FileBufferCursor_retreat) {
  if ($M_sof($y_FileBufferCursor_sof,0)) {
    $I_FileBufferCursor_line_number = 0;
    return;
  }

  --$I_FileBufferCursor_line_number;
  $I_FileBufferCursor_offset =
    $M_links($I_FileBuffer_prev_offset,
             $o_FileBufferCursor_buffer);
}

/*
  SYMBOL: $f_FileBufferCursor_read
    Reads the contents of the line the cursor currently refers to, setting
    $w_FileBuffer_line.

  SYMBOL: $w_FileBuffer_line
    The current or new contents of the line pointed to by the cursor. Updated
    by calls to $f_FileBufferCursor_read.
 */
defun($h_FileBufferCursor_read) {
  $w_FileBufferCursor_line =
    $M_read($w_FileBuffer_line, $o_FileBufferCursor_buffer);
}

/*
  SYMBOL: $f_FileBufferCursor_write
    Writes
 */
defun($h_FileBufferCursor_write) {
  $M_write(0, $o_FileBufferCursor_buffer);
}

/*
  SYMBOL: $f_FileBufferCursor_ins_before
    Inserts a new line before the line this cursor refers to, whose content
    will be $w_FileBufferCursor_new_line. This cursor remains here (though its
    line number will be adjusted as necessary).
 */
defun($h_FileBufferCursor_ins_before) {
  unsigned after = $I_FileBufferCursor_offset;
  unsigned before =
    $M_links($I_FileBuffer_prev_offset,
             $o_FileBufferCursor_buffer);
  $M_insert(0, $o_FileBufferCursor_buffer,
            $I_FileBuffer_prev_offset = before,
            $I_FileBuffer_next_offset = after);
}

/*
  SYMBOL: $f_FileBufferCursor_ins_after
    Inserts a new line after the line this cursor referst to, whose content
    will be $w_FileBufferCursor_new_line. This cursor remains here.
 */
defun($h_FileBufferCursor_ins_after) {
  unsigned before = $I_FileBufferCursor_offset;
  unsigned after =
    $M_links($I_FileBuffer_next_offset,
             $o_FileBufferCursor_buffer);
  $M_insert(0, $o_FileBufferCursor_buffer,
            $I_FileBuffer_prev_offset = before,
            $I_FileBuffer_next_offset = after);
}

/*
  SYMBOL: $f_FileBufferCursor_del
    Deletes the line pointed to by this cursor. The cursor will be shunted or
    invalidated according to $v_FileBufferCursor_shunt_mode.
 */
defun($h_FileBufferCursor_del) {
  $M_delete(0, $o_FileBufferCursor_buffer);
}

/*
  NOTES: FileBuffer Working File Format

  Unlike many editors, Soliloquy's file buffer does not keep the contents of
  the file it is operating on, nor its undo states, in memory, but rather
  journals them to a working file. (Obviously, this is still stored in memory
  for memory-backed "files".)

  At a high level, a FileBuffer is structured into a chain of entities,
  identified by their byte offset into the working file. An entity is
  essentially a single line, plus its undo/redo history. An entity records the
  following data:
  - The byte offset of the previous and next entities in the chain
  - The byte offset of the undo and redo entities for this entity
  - The serial numbers of the undo and redo states
  - The content of the entity

  The working file is stored in plain-text, one entity per line. The first line
  contains file identification; the second contains a single integer, the byte
  offset of the root entity (logical first line of the FileBuffer).

  All integers in the working file are stored as 8-hexit hexadecimal numbers,
  padded at left by zeroes. Each entity consists of a number of fields
  separated by commas; entities are terminated by newline characters. The
  fields are as follows:
    int: previous entity
    int: next entity
    int: previous undo state
    int: previous undo serial number
    int: next redo state
    int: next redo serial number
    characters to newline: line body

  Changing the contents of a line is performed as follows:
  - A new entity is written with the new contents of the line, and the same
    previous/next entries as the original.
  - The new entity's "previous undo" is set to the old entity.
  - The old entity's "next redo" is set to the new entity.
  - The next link of the previous, and the previous link of the next, are made
    to point to the new entity.
  - The next and previous links of the old entity are nulled.

  Deleting a line comprises the following operations:
  - A parent entity is selected. This is the previous entity if present, the
    next otherwise.
  - A new entity identical to the parent is created, except the next or
    previous link which would point to the entity being removed instead mirror
    the corresponding links from the entity being removed.
  - The undo/redo values are set for the new and old entities as described for
    "changing the contents of a line".
  - The next or previous link of the old entity and the entity being deleted
    that does not refer to the opposite of the two is nulled.
  - The appropriate next/previous links of the surrounding entities are updated
    to point to the new entity.

  Inserting a line is similar:
  - A parent entity is selected, as for deleting a line.
  - A copy of the parent, plus the new entity, are created, so that they form
    a chain in the appropriate order, and reference the two entities they sit
    between.
  - Undo/redo information is recorded in the parent entity.
  - The entities outside the chain are linked to the new chain.

  Undo and redo operations work by simply re-linking the existing entity chains
  using the undo/redo lists. Each chain which is not the current state in an
  undo or redo list is terminated by NULL next/previous entries, allowing to
  determine where the ends of each edit are.

  Note that any operations that would require modifying the link of a previous
  entity (not the previous link itself) where the previous entity is NULL must
  instead modify the pointer to the root entity at the begining of the file.

  Note that there is some redudancy in this format, specifically with regard to
  line contents; each line insertion ends up duplicating the contents of the
  line preceeding or proceding it. However, it is felt that the extra space
  usage is preferable to the overhead of an extra level of indirection which
  would be incurred by pointing to a shared content line.
 */

static size_t sizeloc_discard;
/*
  SYMBOL: $f_FileBuffer
    Tracks the contents and undo/redo of a "file buffer", which may be based in
    memory. The constructor will roll the current transaction back if
    construction fails, after calling $f_FileBuffer_destroy().

  SYMBOL: $w_FileBuffer_name
    The base filename for the file buffer, or logical name if
    $y_FileBuffer_is_memory_based.

  SYMBOL: $y_FileBuffer_is_memory_based
    If true, the file represented by this FileBuffer is stored in memory
    instead of on-disk.

  SYMBOL: $p_FileBuffer_file
    A FILE* which backs this FileBuffer.

  SYMBOL: $p_FileBuffer_filebacking
    Pointer set by open_wmemstream to be freed when the FileBuffer is
    destroyed.

  SYMBOL: $lo_FileBuffer_cursors
    A list of FileBufferCursors currently referring into this FileBuffer.

  SYMBOL: $I_FileBuffer_root_offset
    The current offset within the file of the zeroth entity of the file.

  SYMBOL: $o_FileBuffer_end_cursor
    The FileBufferCursor pointing to the end of the file. This should never be
    modified externally; instead, clone it with object_clone().

  SYMBOL: $I_FileBuffer_root_pointer_offset
    The byte offset within the backing file of the pointer to the root entity.

  SYMBOL: $I_FileBuffer_curr_offset
    In operations which read or write FileBuffer entities, the byte offset of
    the entity in question.

  SYMBOL: $I_FileBuffer_next_offset $I_FileBuffer_prev_offset
    Reflects the next/previous entity links when reading an entity, or the
    values to write when writing an entity.

  SYMBOL: $I_FileBuffer_undo_offset $I_FileBuffer_redo_offset
    Reflects the undo/redo entity links when reading an entity, or the values
    to write when writing an entity.

  SYMBOL: $I_FileBuffer_undo_serial_number $I_FileBuffer_redo_serial_number
    The serial number of the undo/redo states for an entity, used when reading
    or writing an entity. These are only considered relevant of the
    corresponding offset is non-NULL.

  SYMBOL: $I_FileBuffer_edit_serial_number
    The serial number of the current edit operation, for purposes of tracking
    undo states. Edits with the same serial number will be undone or redone as
    a unit.

  SYMBOL: $w_FileBuffer_entity_contents
    Reflects the string contents of an entity when reading or writing an
    entity.
 */
defun($h_FileBuffer) {
  $I_FileBuffer_edit_serial_number = 0;

  if ($y_FileBuffer_is_memory_based) {
    /* Do this later 
    $p_FileBuffer_file = open_memstream((char**)&$p_FileBuffer_filebacking,
                                         &sizeloc_discard);
    */
    $p_FileBuffer_file = fopen("sol.out", "w+");
  } else {
    fprintf(stderr, "File-based FileBuffers not yet supported.\n");
    exit(1);
  }

  if (!$p_FileBuffer_file) {
    $m_destroy();
    $v_rollback_type = $u_FileBuffer;
    $s_rollback_reason = strerror(errno);
    tx_rollback();
  }

  // Write header identifying file type, which also means that 0 is never a
  // valid offset.
  fputws(L"Soliloquy Autosave / Edit Log\n", $p_FileBuffer);

  $I_FileBuffer_root_pointer_offset = ftell($p_FileBuffer_file);
  $m_write_root_pointer();
  $I_FileBuffer_root_offset = ftell($p_FileBuffer_file);
  $m_write_root_pointer();
  $M_write_entity(0,0,
                  $I_FileBuffer_curr_offset = $I_FileBuffer_root_offset,
                  $I_FileBuffer_next_offset = 0,
                  $I_FileBuffer_prev_offset = 0,
                  $I_FileBuffer_undo_offset = 0,
                  $I_FileBuffer_redo_offset = 0,
                  $w_FileBuffer_entity_contents = L"");
  $o_FileBuffer_end_cursor =
    $c_FileBufferCursor($v_FileBufferCursor_shunt_mode =
                          $u_shunt_downward,
                        $o_FileBufferCursor_buffer = $o_FileBuffer);
}

/*
  SYMBOL: $f_FileBuffer_destroy
    Frees the resources used by this FileBuffer.
 */
defun($h_FileBuffer_destroy) {
  if ($p_FileBuffer_file)
    fclose($p_FileBuffer_file);
  if ($p_FileBuffer_filebacking)
    free($p_FileBuffer_filebacking);
}

defun($h_FileBuffer_read_entity) {
  if (-1 == fseek($p_FileBuffer_file, $I_FileBuffer_curr_offset, SEEK_SET)) {
    $v_rollback_type = $u_FileBuffer;
    $s_rollback_reason = strerror(errno);
    tx_rollback();
  }

  // Read the links in
  if (6 != fscanf($p_FileBuffer_file, "%x,%x,%x,%x,%x,%x,",
                  &$I_FileBuffer_prev_offset,
                  &$I_FileBuffer_next_offset,
                  &$I_FileBuffer_undo_offset,
                  &$I_FileBuffer_undo_serial_number,
                  &$I_FileBuffer_redo_offset,
                  &$I_FileBuffer_redo_serial_number)) {
    // Something went wrong
    $v_rollback_type = $u_FileBuffer;
    $s_rollback_reason = "Corrupted working file";
    tx_rollback();
  }

  // Count the number of characters in the entity
  unsigned size = 0;
  long contents_start = ftell($p_FileBuffer_file);
  wint_t curr;
  do {
    curr = fgetwc($p_FileBuffer_file);
    ++size;
  } while (curr != WEOF && curr != L'\n');

  if (curr == WEOF) {
    // Unexpected end of file or error
    $v_rollback_type = $u_FileBuffer;
    $s_rollback_reason = ferror($p_FileBuffer_file)?
      strerror(errno) : "Truncated working file";
    tx_rollback();
  }

  // Size includes the term NUL at this point, since the terminating \n was
  // also counted in the loop above.
  mwstring dst = gcalloc(size);
  $w_FileBuffer_entity_contents = dst;
  if (-1 == fseek($p_FileBuffer_file, contents_start, SEEK_SET)) {
    $v_rollback_type = $u_FileBuffer;
    $s_rollback_reason = strerror(errno);
    tx_rollback();
  }

  for (unsigned i = 0; i < size-1; ++i) {
    curr = fgetwc($p_FileBuffer_file);
    if (curr == WEOF) {
      // Error (or someone truncated the file)
      $v_rollback_type = $u_FileBuffer;
      $s_rollback_reason = ferror($p_FileBuffer_file)?
        strerror(errno) : "Truncated working file";
      tx_rollback();
    }

    dst[i] = curr;
  }
}
