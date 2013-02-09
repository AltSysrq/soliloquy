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
#include "message.slc"
#include "key_dispatch.h"
#include "face.h"

/*
  TITLE: Message Posting Interface
  OVERVIEW: Provides functions to display various types of messages to the user
    in the best way given the current context. Typically, the behaviour is
    based on whether there is currently a transcript.
 */

/*
  SYMBOL: $c_Interruption
    An Activity which interrupts the user to deliver a message. It goes away
    after $I_Interruption_key_count keystrokes, or when aborted (C-g). All
    non-abort keystrokes pass through to the underlying activity.

  SYMBOL: $I_Interruption_key_count
    The number of keystrokes before an Interruption goes away.

  SYMBOL: $I_Interruption_keys_so_far
    The number of keystrokes this Interruption has seen.

  SYMBOL: $q_Interruption_text
    The text to display in this Interruption.

  SYMBOL: $lp_Interruption_keymap
    The keymap specific to the Interruption class.
 */
subclass($c_Activity, $c_Interruption)
class_keymap($c_Interruption, $lp_Interruption_keymap, $llp_Activity_keymap)
ATSINIT {
  bind_kp($lp_Interruption_keymap, NULL, KEYBINDING_DEFAULT, NULL, $m_key);
}

defun($h_Interruption) {
  $m_update_echo_area();
}

advise_before_superconstructor($h_Interruption) {
  $y_Activity_on_top = true;
}

/*
  SYMBOL: $f_Interruption_key
    Registers a keystroke, destroying the Interruption if it has reached its
    key count.
 */
defun($h_Interruption_key) {
  ++$I_Interruption_keys_so_far;
  if ($I_Interruption_keys_so_far >= $I_Interruption_key_count)
    $m_destroy();
  else
    $m_update_echo_area();

  $y_key_dispatch_continue = true;
}

/*
  SYMBOL: $f_Interruption_get_echo_area_contents
    Sets the echo area contents to the message for this Interruption.
 */
defun($h_Interruption_get_echo_area_contents) {
  qchar tail[$I_Interruption_key_count+1];
  tail[$I_Interruption_key_count] = 0;
  for (unsigned i = 0; i < $I_Interruption_key_count; ++i)
    tail[i] = L'.';

  $q_Workspace_echo_area_contents = qstrap($q_Interruption_text,
                                           tail);
  $i_Workspace_echo_area_cursor =
    qstrlen($q_Interruption_text) + $I_Interruption_keys_so_far;
}

/*
  SYMBOL: $f_Interruption_is_echo_enabled
    Sets $y_Workspace_is_echo_enabled to true.
 */
defun($h_Interruption_is_echo_enabled) {
  $y_Workspace_is_echo_enabled = true;
}

/*
  SYMBOL: $f_Interruption_get_echo_area_meta
    Prepends a '!' to the underlying activities' echo area meta.
 */
defun($h_Interruption_get_echo_area_meta) {
  if ($lo_echo_area_activities) {
    object next = $lo_echo_area_activities->car;
    let($lo_echo_area_activities, $lo_echo_area_activities->cdr);
    $M_get_echo_area_meta(0, next);
  }

  qchar bang[2] = { L'!', 0 };
  $q_Workspace_echo_area_meta = qstrap(bang, $q_Workspace_echo_area_meta);
}

/*
  SYMBOL: $f_Interruption_abort
    Destroys this Interruption.
 */
defun($h_Interruption_abort) {
  $m_destroy();
}

ATSINIT {
  $I_message_error_face = mkface("+fR");
  $I_message_notice_face = mkface("+fg");
}

static void message_common(face, wchar_t, unsigned);
/*
  SYMBOL: $f_message_error
    Produces an error message. With a transcript, the message is simply
    appended to it. Otherwise, the user is interrupted.

  SYMBOL: $I_message_error_face
    The face to apply to error messages, when not already formatted.

  SYMBOL: $w_message_text $q_message_text
    The message to display in any of the f_message functions. If
    $q_message_text is NULL, $w_message_text is formatted into $q_message_text
    with the appropriate face. Both of these are NULLed when a message
    function returns.

  SYMBOL: $I_error_key_count
    The number of keystrokes to dismiss an error Interruption.
 */
defun($h_message_error) {
  message_common($I_message_error_face, L'!', $I_error_key_count);
}

/*
  SYMBOL: $f_message_notice
    Produces a notice. This is a purely informational message that is added to
    the transcript if possible, otherwise shown to the user for a short time.

  SYMBOL: $I_message_notice_face
    The face to apply to notices, when not already foramtted.

  SYMBOL: $I_notice_key_count
    The number of keystrokes to dismiss a notice Interruption.
 */
defun($h_message_notice) {
  message_common($I_message_notice_face, L':', $I_notice_key_count);
}

static void message_common(face message_face, wchar_t mchar,
                           unsigned key_count) {
  if (!$q_message_text) {
    $q_message_text = apply_face_str(message_face,
                                     wstrtoqstr($w_message_text));
  }

  qchar meta[$i_line_meta_width+1];
  for (int i = 0; i < $i_line_meta_width; ++i)
    meta[i] = apply_face($I_message_error_face,
                         i < $i_line_meta_width-2? mchar : ' ');
  meta[$i_line_meta_width] = 0;

  if ($o_Transcript) {
    let($lo_Transcript_output,
        cons_o($c_RenderedLine(
                 $q_RenderedLine_meta = qstrdup(meta),
                 $q_RenderedLine_body = $q_message_text),
               NULL));
    $m_append();
  } else if ($o_Workspace) {
    $c_Interruption($q_Interruption_text =
                      qstrap(meta, $q_message_text),
                    $I_Interruption_key_count = key_count);
  }

  $q_message_text = NULL;
  $w_message_text = NULL;
}

STATIC_INIT_TO($I_error_key_count, 5)
STATIC_INIT_TO($I_notice_key_count, 1)
