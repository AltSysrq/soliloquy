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

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

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

  SYMBOL: $y_FileBuffer_modified
    If true, this FileBuffer has been modified relative to the latest on-disk
    version.

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

  SYMBOL: $lo_buffers
    A list of all FileBuffer-like objects in existence.
 */
defun($h_FileBuffer) {
  if (!$p_shared_undo_log) {
    $p_shared_undo_log = tmpfile();
    //$p_shared_undo_log = fopen("sol.out", "w+b");
    if (!$p_shared_undo_log)
      tx_rollback_errno($u_FileBuffer);

    if (-1 == fputs("Soliloquy Undo Journal\n", $p_shared_undo_log)) {
      int old_errno = errno;
      fclose($p_shared_undo_log);
      $p_shared_undo_log = NULL;

      errno = old_errno;
      tx_rollback_errno($u_FileBuffer);
    }
  }

  // See whether we can access the file and write to the file. If it doesn't
  // exist, that's OK; we just consider it already modified.
  if (!$y_FileBuffer_memory_backed) {
    string filename = wstrtocstr($w_FileBuffer_filename);
    if (-1 == access(filename, F_OK)) {
      // Doesn't exist, that's fine
      $y_FileBuffer_modified = true;
      // Seed the initial buffer contents
      let($y_FileBuffer_memory_backed, true);
      $m_access();
    } else if (-1 == access(filename, R_OK)) {
      // Not readable, not OK
      tx_rollback_errno($u_FileBuffer);
    } else if (-1 == access(filename, W_OK)) {
      // Read-only
      $y_FileBuffer_readonly = true;
    }
  }

  lpush_o($lo_buffers, $o_FileBuffer);
}

/*
  SYMBOL: $f_FileBuffer_destroy
    Destroys this file buffer, and anything attached to it.

  SYMBOL: $lo_FileBuffer_attachments
    A list of destroyable objects "attached" to this FileBuffer, which should
    be destroyed when it is.
 */
defun($h_FileBuffer_destroy) {
  each_o($lo_FileBuffer_attachments,
         lambdav((object that), $M_destroy(0,that)));
  $lo_buffers = lrm_o($lo_buffers, $o_FileBuffer);


  // If we have been modified, there may be an autosave file on-disk. Remove it
  // if it is there.
  if (!$y_FileBuffer_memory_backed && $y_FileBuffer_modified) {
    // Ignore the return value; if it failed, there isn't anything we can do
    // about it, and we can't give a terribly informative message to the user
    // about the problem.
    unlink(wstrtocstr(wstrap($w_FileBuffer_filename, L"#")));
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
      // Re-read the file. If the buffer is currently modified, read from the
      // autosave file instead, since it contains the actual current contents.
      wstring wfilename = $w_FileBuffer_filename;
      if ($y_FileBuffer_modified)
        wfilename = wstrap(wfilename, L"#");
      string filename = wstrtocstr(wfilename);

      FILE* input = fopen(filename, "r");
      mstring line = NULL;
      size_t line_len = 0;
      if (!input)
        tx_rollback_errno($u_FileBuffer);

      $aw_FileBuffer_contents = dynar_new_w();

      /* Reading the file in as a narrow string, then converting the lines to
       * wstrings, has the advantage that we can fall back to ISO-8859-1 if
       * decoding fails, whereas using, eg, fgetwc, we simply get an error and
       * have no good way to fall back. The disadvantage is that this can't
       * handle files encoded in encodings like UCS-4 or UTF-16, since the NUL
       * "characters" will prematurely terminate the string. To handle such
       * encodings, a subclass of FileBuffer will be needed which implements
       * reload differently.
       */
      while (-1 != getline(&line, &line_len, input)) {
        // Delete the trailing newline character
        for (unsigned i = 0; line[i]; ++i)
          if (line[i] == L'\n')
            line[i] = 0;
        dynar_push_w($aw_FileBuffer_contents,
                     cstrtowstr(line));
      }

      if (line)
        free(line);

      if (ferror(input)) {
        int err = errno;
        fclose(input);
        errno = err;
        tx_rollback_errno($u_FileBuffer);
      }
      fclose(input);
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
  SYMBOL: $f_FileBuffer_write_autosave
    If this FileBuffer is modified and not memory backed, writes the current
    contents of the buffer to "NAME#", where "NAME" is the base filename. The
    transaction is rolled back if this fails. There is no effect if this buffer
    is memory backed or if it has not been modified.

  SYMBOL: $y_FileBuffer_suppress_fsync_on_autosave
    If true, fsync() will not be called to ensure that a buffer's contents have
    been flushed to disk on autosave. This will make autosave less useful
    against power or system failure, since there is no guarantee that the
    autosave file will actually be meaningful.
 */
defun($h_FileBuffer_write_autosave) {
  if ($y_FileBuffer_modified && !$y_FileBuffer_memory_backed) {
    $m_access();

    wstring filename = wstrap($w_FileBuffer_filename, L"#");
    FILE* output = fopen(wstrtocstr(filename), "w");
    if (!output)
      tx_rollback_errno($u_FileBuffer);

    for (unsigned i = 0; i < $aw_FileBuffer_contents->len; ++i) {
      if (-1 == fprintf(output, "%ls\n", $aw_FileBuffer_contents->v[i])) {
        int err = errno;
        fclose(output);
        errno = err;
        tx_rollback_errno($u_FileBuffer);
      }
    }

    fflush(output);
    if (!$y_FileBuffer_suppress_fsync_on_autosave)
      fsync(fileno(output));
    fclose(output);
  }
}

/*
  SYMBOL: $f_FileBuffer_release
    Releases $ao_FileBuffer_meta and $aw_FileBuffer_contents for this
    FileBuffer.
 */
defun($h_FileBuffer_release) {
  //Can't release a memory-backed buffer
  if ($y_FileBuffer_memory_backed) return;

  //Need to write to autosave file first, if applicable
  $m_write_autosave();

  $ao_FileBuffer_meta = NULL;
  $aw_FileBuffer_contents = NULL;
}

/*
  SYMBOL: $f_FileBuffer_require_writable
    Rolls the current transaction back if this FileBuffer is readonly.

  SYMBOL: $y_FileBuffer_readonly
    If true, edits are not permitted to this FileBuffer.
 */
defun($h_FileBuffer_require_writable) {
  if ($y_FileBuffer_readonly) {
    $v_rollback_type = $u_FileBuffer;
    $s_rollback_reason = "Buffer is read-only";
    tx_rollback();
  }
}

STATIC_INIT_TO($I_FileBuffer_default_file_mode, 0644);
/*
  SYMBOL: $f_FileBuffer_save
    Saves the current contents of the buffer to the buffer's filename. This has
    no effect if the buffer is unmodified, or if it is memory-backed. Saving
    the file is a three-step process:
    - The autosave file is written (to "NAME#").
    - The autosave file is given the same attributes as the current file (if it
      exists); otherwise, the attributes are derived from
      $I_FileBuffer_default_file_mode.
    - Unless $y_FileBuffer_suppress_fsync_on_save, fsync() is called on the
      autosave file.
    - The current file (if it exists) is renamed to "NAME~", with the same
      attributes, overwriting that file if it already exists. This step is not
      performed if $y_FileBuffer_suppress_backup is true.
    - The autosave file is renamed onto the base file. This is guaranteed by
      POSIX to be atomic.
    - The buffer is set to unmodified.

  SYMBOL: $y_FileBuffer_suppress_backup
    When saving a FileBuffer, don't rename the original file to a backup
    first.

  SYMBOL: $y_FileBuffer_suppress_fsync_on_save
    When saving a FileBuffer, don't call fsync() on the output file. This means
    that if your system dies (eg, power failure, kernel panic), you might lose
    the contents of the file, but you will get somewhat better speed and lower
    power consumption. See also $y_FileBuffer_suppress_fsync_on_autosave.

  SYMBOL: $I_FileBuffer_default_file_mode
    The file mode to apply to newly-created files.

  SYMBOL: $I_FileBuffer_saved_undo_offset
    The undo offset of the FileBuffer at the last save.
 */
defun($h_FileBuffer_save) {
  if (!$y_FileBuffer_modified || $y_FileBuffer_memory_backed)
    return;

  wstring wbasename = $w_FileBuffer_filename;
  wstring wbakname = wstrap(wbasename, L"~");
  wstring wasname = wstrap(wbasename, L"#");
  string basename = wstrtocstr(wbasename);
  string bakname = wstrtocstr(wbakname);
  string asname = wstrtocstr(wasname);

  $m_write_autosave();

  // Set permissions on autosave file
  mode_t mode;
  struct stat orig_info;
  bool orig_exists;

  if (-1 != stat(basename, &orig_info)) {
    mode = orig_info.st_mode;
    orig_exists = true;
  } else if (errno == ENOENT) {
    // The original file does not yet exist; use the default mode
    mode = $I_FileBuffer_default_file_mode;
    orig_exists = false;
  } else {
    // Something else went wrong.
    tx_rollback_errno($u_FileBuffer);
  }

  if (-1 == chmod(asname, mode))
    tx_rollback_errno($u_FileBuffer);

  // fsync() the autosave file if requested
  if (!$y_FileBuffer_suppress_fsync_on_save) {
    int fd = open(asname, O_WRONLY);
    // We shouldn't ever get -1 from the call to open(), but handle that case
    // gracefully.
    if (-1 != fd) {
      fsync(fd);
      close(fd);
    }
  }

  // Move original file to the backup filename
  if (!$y_FileBuffer_suppress_backup && orig_exists) {
    if (-1 == rename(basename, bakname))
      tx_rollback_errno($u_FileBuffer);
  }

  // Move the autosave file to the basename
  if (-1 == rename(asname, basename))
    tx_rollback_errno($u_FileBuffer);

  $y_FileBuffer_modified = false;
  $I_FileBuffer_saved_undo_offset = $I_FileBuffer_undo_offset;
  tx_write_through($y_FileBuffer_modified);
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
  $m_require_writable();
  $m_access();

  wchar_t undo_type =
    ($y_FileBuffer_continue_undo? L'&' : L'@');
  $y_FileBuffer_continue_undo = false;
  // Since we're making a new edit, this destroys the redo trail.
  $lI_FileBuffer_redo_trail = NULL;

  // Cap ndeletions if it would run past the end of the buffer
  if ($I_FileBuffer_edit_line + $I_FileBuffer_ndeletions >
      $aw_FileBuffer_contents->len)
    $I_FileBuffer_ndeletions =
      $aw_FileBuffer_contents->len - $I_FileBuffer_edit_line;

  //Abort if out of range
  if ($I_FileBuffer_edit_line > $aw_FileBuffer_contents->len) {
    $v_rollback_type = $u_FileBuffer;
    $s_rollback_reason = "$I_FileBuffer_edit_line out of range";
    tx_rollback();
  }

  list_w insertions = $lw_FileBuffer_replacements;

  unsigned long long now = time(0);
  unsigned prev_undo = $I_FileBuffer_undo_offset;

  $I_FileBuffer_undo_offset = ftell($p_shared_undo_log);
  if (!~$I_FileBuffer_undo_offset)
    tx_rollback_errno($u_FileBuffer);

  //Write the header for this undo record
  if (-1 ==
      fprintf($p_shared_undo_log, "%lc%X,%X,%llX:%ls\n",
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
    if (-1 == fprintf($p_shared_undo_log, "-%ls\n",
                       $aw_FileBuffer_contents->v[i + $I_FileBuffer_edit_line]))
      tx_rollback_errno($u_FileBuffer);

  //Write insertions
  for (list_w curr = insertions; curr; curr = curr->cdr)
    if (-1 == fprintf($p_shared_undo_log, "+%ls\n", curr->car))
      tx_rollback_errno($u_FileBuffer);

  $y_FileBuffer_modified = true;

  $m_raw_edit();
}

/*
  SYMBOL: $f_FileBuffer_read_undo_entry $I_FileBuffer_read_undo_entry
    Reads the undo entry in the undo journal at the given offset, setting
    $I_FileBuffer_undo_time, $I_FileBuffer_prev_undo, and
    $y_FileBuffer_continue_undo according to the data stored in the
    headers. $I_FileBuffer_edit_line, $I_FileBuffer_ndeletions, and
    $lw_FileBuffer_replacements are set up to be suitable for a call to
    $f_FileBuffer_raw_edit(), given $z_FileBuffer_undo_deletion_char.

  SYMBOL: $I_FileBuffer_prev_undo
    The "previous undo offset" of the record read by
    $f_FileBuffer_read_undo_entry().

  SYMBOL: $I_FileBuffer_undo_time
    The timestamp of the undo record read by $f_FileBuffer_read_undo_entry().

  SYMBOL: $z_FileBuffer_undo_deletion_char
    Either '+' or '-'. When reading an undo record, lines beginning with this
    character are treated as deletions; the contrasponding character is treated
    as an insertion.
 */
defun($h_FileBuffer_read_undo_entry) {
  FILE* journal = $p_shared_undo_log;
  if (-1 == fseek(journal, $I_FileBuffer_read_undo_entry, SEEK_SET)) {
    int err = errno;
    //Try to seek back to where we were
    fseek(journal, 0, SEEK_END);

    errno = err;
    tx_rollback_errno($u_FileBuffer);
  }

  // Read the header in
  char type;
  unsigned long long when;
  int nscanned = fscanf(journal, "%c%X,%X,%llX:",
                        &type,
                        &$I_FileBuffer_prev_undo,
                        &$I_FileBuffer_edit_line,
                        &when);
  $I_FileBuffer_undo_time = when;

  if (nscanned != 4 || (type != '&' && type != '@')) {
    int err = errno;
    fseek(journal, 0, SEEK_END);
    errno = err;

    tx_rollback_merrno($u_FileBuffer, nscanned, "Corrupt undo journal");
  }

  $y_FileBuffer_continue_undo = (type == '&');
  $I_FileBuffer_ndeletions = 0;
  $lw_FileBuffer_replacements = NULL;

  char* line = NULL;
  size_t line_size = 0;
  //Skip the rest of the current line
  getline(&line, &line_size, journal);
  //Read in the rest of the record
  while (-1 != getline(&line, &line_size, journal)) {
    // Line will always be at least one byte long (for the term NUL)
    if (line[0] != '+' && line[0] != '-') 
      // End of this record
      break;

    //Trim off the trailing newline
    line[strlen(line)] = 0;

    // Classify it as insertion or deletion
    if (line[0] == (char)$z_FileBuffer_undo_deletion_char)
      ++$I_FileBuffer_ndeletions;
    else
      lpush_w($lw_FileBuffer_replacements, cstrtowstr(line+1));
  }

  if (line)
    free(line);

  //Seek back to the end
  {
    int err = errno;
    fseek(journal, 0, SEEK_END);
    errno = err;
  }

  if (ferror(journal))
    tx_rollback_errno($u_FileBuffer);

  // Put the replacements back into the correct order
  $lw_FileBuffer_replacements = lrev_w($lw_FileBuffer_replacements);
}

/*
  SYMBOL: $f_FileBuffer_undo
    Undoes changes to this FileBuffer by one step. The transaction is rolled
    back if anything fails, or if there is no undo information.

  SYMBOL: $lI_FileBuffer_redo_trail
    List of file offsets for redo states within the journal. Each time a record
    is undone, it is pushed into this list.
 */
defun($h_FileBuffer_undo) {
  if (!$I_FileBuffer_undo_offset) {
    $s_rollback_reason = "No more undo information";
    $v_rollback_type = $u_FileBuffer;
    tx_rollback();
  }

  do {
    lpush_I($lI_FileBuffer_redo_trail, $I_FileBuffer_undo_offset);
    $z_FileBuffer_undo_deletion_char = L'+';
    $M_read_undo_entry(0,0,
                       $I_FileBuffer_read_undo_entry =
                         $I_FileBuffer_undo_offset);
    $I_FileBuffer_undo_offset = $I_FileBuffer_prev_undo;
    $m_raw_edit();
  } while ($y_FileBuffer_continue_undo && $I_FileBuffer_undo_offset);

  $y_FileBuffer_modified =
    ($I_FileBuffer_undo_offset != $I_FileBuffer_saved_undo_offset);
}

/*
  SYMBOL: $f_FileBuffer_redo
    Redoes the changes to this FileBuffer by one step. The transaction is
    rolled back if anything fails, or if there is no redo information. This is
    a notably more expensive operation than $f_FileBuffer_undo().
 */
defun($h_FileBuffer_redo) {
  if (!$lI_FileBuffer_redo_trail) {
    $s_rollback_reason = "No more redo information";
    $v_rollback_type = $u_FileBuffer;
    tx_rollback();
  }

  bool has_redone_anything = false;
  while ($lI_FileBuffer_redo_trail) {
    // We must read the next entry before we can decide how to handle it.
    $I_FileBuffer_read_undo_entry = $lI_FileBuffer_redo_trail->car;
    $z_FileBuffer_undo_deletion_char = L'-';
    $m_read_undo_entry();

    //If this is the first entry we've seen, we always replay it. Otherwise, we
    //only do so if it is a continuation of the previous.
    if (!has_redone_anything || $y_FileBuffer_continue_undo) {
      $I_FileBuffer_undo_offset = lpop_I($lI_FileBuffer_redo_trail);
      $m_raw_edit();
    } else {
      break;
    }

    has_redone_anything = true;
  }

  $y_FileBuffer_modified =
    ($I_FileBuffer_undo_offset != $I_FileBuffer_saved_undo_offset);
}

/*
  SYMBOL: $f_FileBuffer_raw_edit
    Applies edit changes (as described in $f_FileBuffer_edit) to the buffer,
    without writing to the undo log.
 */
defun($h_FileBuffer_raw_edit) {
  $m_access();

  unsigned ndeletions = $I_FileBuffer_ndeletions;
  unsigned ninsertions = llen_w($lw_FileBuffer_replacements);
  unsigned nreplacements =
    ndeletions > ninsertions? ninsertions : ndeletions;
  list_w insertions = $lw_FileBuffer_replacements;

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
