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
#include "symbol_chord.slc"

#include <time.h>

#include "../kb_layout_xlate.h"

/*
  TITLE: Symbol Chord Mode
  OVERVIEW: Extends LineEditor to support the entering of common programming
    symbols using easily-accessible key chords.
 */

/*
  SYMBOL: $u_symbol_chord_mode
    Identifies hooks added by symbol chord mode.

  SYMBOL: $y_LineEditor_symbol_chord_mode_default
  SYMBOL: $y_LineEditor_symbol_chord_mode
    Controls whether symbol-chord-mode is active for the current LineEditor.

  SYMBOL: $I_LineEditor_symbol_chord_duration_ms
    The maximum number of milliseconds which may ellapse between keystrokes for
    a pair of characters to be considered a chord.

  SYMBOL: $w_LineEditor_symbol_chords
    String of chords recognised by symbol-chord-mode. The length of this string
    must be a multiple of three. The string is structured as a pair of input
    characters followed by the character to map to; for example, "xq~" would
    specify that a '~' should be input when 'x' and 'q' are pressed
    simultaneously. The order of the input characters is irrelevant. Input
    characters are normalised to US QWERTY, so that the default remains
    reasonable across keyboard layouts; keep this in mind if you customise this
    variable.
 */
ATSINIT {
  $y_LineEditor_symbol_chord_mode_default = true;
  $I_LineEditor_symbol_chord_duration_ms = 35;
  $w_LineEditor_symbol_chords =
    L"df(jk)er{ui}cv[m,]nm]"
     "as+l;-"
     "sd<kl>"
     "af/j;*"
     "sf|jl&"
     "ad!k;#"
     "fj0fk1"
     "fl2f;3"
     "dj4dk5"
     "dl6d;7"
     "sj8sk9"
     "sl%s;^"
     "aj@ak$"
     "al=a;_";
}

defmode($c_LineEditor, $u_symbol_chord_mode,
        $y_LineEditor_symbol_chord_mode,
        $y_LineEditor_symbol_chord_mode_default)

/*
  SYMBOL: $u_input_preprocessing
    Hook class for hooks which do something special with user input, typically
    before or after the primary handler runs.

  SYMBOL: $z_LineEditor_symbol_chord_first
    If non-NUL, the character input by the user in the previous command which
    may result in a symbol chord being processed. It has not yet undergone
    QWERTYfication.

  SYMBOL: $I_LineEditor_symbol_chord_prev
    The "time" of the last potential character input within this line editor,
    for purposes of checking for symbol chords. It is determined by calling
    clock_gettime() on CLOCK_MONOTONIC, and calculating
      (tv_sec*1000 + ((unsigned long)tv_nsec/1000000))
    It is thus only really useful for performing comparisons, since it wraps
    around every 49.7 days.
 */
mode_adv_before($u_input_preprocessing, $h_LineEditor_self_insert) {
  if (!is_nc_char($x_Terminal_input_value))
    return;

  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    // If we can't tell the time, there's nothing we can do
    return;

  unsigned now = ts.tv_sec*1000 + ((unsigned long)ts.tv_nsec/1000000);

  if ($z_LineEditor_symbol_chord_first &&
      now - $I_LineEditor_symbol_chord_prev <=
        $I_LineEditor_symbol_chord_duration_ms) {
    // Qwertify both of them
    wchar_t a = qwertify($z_LineEditor_symbol_chord_first);
    wchar_t b = qwertify($x_Terminal_input_value);

    for (wstring map = $w_LineEditor_symbol_chords; *map; map += 3) {
      if ((map[0] == a && map[1] == b) ||
          (map[0] == b && map[1] == a)) {
        // Remove the character before point
        if ($i_LineEditor_point > 0 &&
            $az_LineEditor_buffer->v[$i_LineEditor_point-1] ==
              $z_LineEditor_symbol_chord_first) {
          --$i_LineEditor_point;
          $y_LineEditor_edit_is_minor = true;
          $m_push_undo();
          dynar_erase_z($az_LineEditor_buffer, $i_LineEditor_point, 1);

          // Replace with mapping
          $x_Terminal_input_value = map[2];

          // Done
          $z_LineEditor_symbol_chord_first = 0;
          return;
        }
      }
    }
  }

  // Either the previous wasn't a match, or this is the start of a new chord.
  $I_LineEditor_symbol_chord_prev = now;
  $z_LineEditor_symbol_chord_first = $x_Terminal_input_value;
}
