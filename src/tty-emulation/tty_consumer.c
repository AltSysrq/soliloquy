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
#include "tty_consumer.slc"
#include <limits.h>
#include <unistd.h>

/*
  TITLE: TTY Emulation Consumer
  OVERVIEW: Provides a class to read bytes from a file descriptor and feed
    up-converted characters into the TTY Emulator.
 */

/*
  SYMBOL: $c_TtyConsumer
    Consumer which pulls individual bytes from the input stream, up-converts
    them to wide characters, and passes them onto a TtyEmulator. The file
    descriptor MUST be opened in non-blocking mode.

  SYMBOL: $p_TtyConsumer_mbstate
    A mbstate_t* which stores the current multibyte state.

  SYMBOL: $S_TtyConsumer_wchbuf $I_TtyConsumer_wchbuf
    An array of MB_LEN_MAX chars which stores the current multibyte
    sequence. $I_TtyConsumer_wchbuf points to the first index currently
    unused.
 */
subclass($c_Consumer, $c_TtyConsumer)
defun($h_TtyConsumer) {
  $p_TtyConsumer_mbstate = new(mbstate_t);
  $S_TtyConsumer_wchbuf = gcalloc(MB_LEN_MAX);
  $I_TtyConsumer_wchbuf = 0;
}

/*
  SYMBOL: $f_TtyConsumer_read
    Reads bytes from the file descriptor until it would block or hits EOF,
    up-converting them to wide characters and passing them onto the underlying
    emulator. On EOF, calls $m_destroy().

  SYMBOL: $o_TtyConsumer_emulator
    The TtyEmulator driven by this TtyConsumer.
 */
defun($h_TtyConsumer_read) {
  char byte;
  ssize_t nread;
  while (0 < (nread = read($i_Consumer_fd, &byte, 1))) {
    // Write into the wch buffer and advance
    $S_TtyConsumer_wchbuf[$I_TtyConsumer_wchbuf++] = byte;

    // Try to convert the buffer to a wide character
    wchar_t wch;
    size_t ret = mbrtowc(&wch, $S_TtyConsumer_wchbuf, $I_TtyConsumer_wchbuf,
                         $p_TtyConsumer_mbstate);
    // Don't consider the sequence incomplete if we've hit MB_LEN_MAX.
    // This can occur with shift-based encodings when we encounter redundant
    // shifts.
    if (ret == (size_t)-2 && $I_TtyConsumer_wchbuf < MB_LEN_MAX) {
      // Incomplete sequence
      continue;
    } else if (ret == (size_t)-1 || ret == (size_t)-2) {
      // Sequence too long or invalid sequence; reset
      $I_TtyConsumer_wchbuf = 0;
      memset($p_TtyConsumer_mbstate, 0, sizeof(mbstate_t));
      continue;
    } else {
      // Success
      // Shift the buffer back by the number of bytes consumed
      // (Assume $I_TtyConsumer_wchbuf if we read a NUL character, which causes
      //  mbrtowc() to return 0.)
      if (!ret)
        ret = $I_TtyConsumer_wchbuf;

      memmove($S_TtyConsumer_wchbuf,
              $S_TtyConsumer_wchbuf+ret,
              $I_TtyConsumer_wchbuf - ret);
      $I_TtyConsumer_wchbuf -= ret;

      // Hand off to the emulator
      $M_addch(0, $o_TtyConsumer_emulator, $z_TtyEmulator_wch = wch);
    }
  }

  if (!nread)
    // EOF
    $m_destroy();
}

/*
  SYMBOL: $f_TtyConsumer_destroy
    Releases the underlying TtyEmulator, then calls $f_Consumer_destroy().
 */
defun($h_TtyConsumer_destroy) {
  $M_release(0, $o_TtyConsumer_emulator);
  $f_Consumer_destroy();
}
