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
#include "view.slc"

#include "inc_ncurses.h"

#include "key_dispatch.h"
#include "face.h"
#include "interactive.h"

/*
  TITLE: Terminal/Workspace View Management
  OVERVIEW: Pairs Terminals wtih Workspaces, tracking the location of cut (both
    within the workspace and on the screen).
*/

/*
  SYMBOL: $c_View
    Binds workspace-specific data for each workspace to a Terminal.

  SYMBOL: $i_View_cut_on_screen
    The line location of cut on the Terminal. 0 is the top of the leftmost
    text column.

  SYMBOL: $i_View_cut_in_workspace
    The index of the last line in the workspace backing.

  SYMBOL: $u_View_Backing_change_notify
    Identity of the change notification hook used by View, to find out when
    portions of the view must be updated.

  SYMBOL: $o_View_workspace
    The workspace bound to this View.

  SYMBOL: $o_View_terminal
    The terminal this View is bound to.

  SYMBOL: $lo_Terminal_views
    A listmap of all Views bound to the Terminal, keyed by Workspace.

  SYMBOL: $lo_Workspace_views
    A list of all Views bound to the Workspace.

  SYMBOL: $i_View_cols
    The number of text columns present in the view. This is not character
    columns, but text columns; eg, a screen 180 characters wide might have two
    text columns.

  SYMBOL: $i_View_rows
    The number of logical rows displayable within the View.

  SYMBOL: $o_Terminal_current_view
    The currently-active view of the Terminal.
 */
defun($h_View) {
  $i_View_cut_in_workspace = $($($o_View_workspace, $o_Workspace_backing),
                               $ao_Backing_lines)->len;
  $o_View_terminal = $o_Terminal;
  $$($($o_View_workspace,$o_Workspace_backing)) {
    add_hook_obj(&$h_Backing_alter, HOOK_AFTER,
                 $u_View_Backing_change_notify, $u_View,
                 $f_View_backing_changed, $o_View,
                 NULL);
  }
  $$($o_View_workspace) {
    add_hook_obj(&$h_Workspace_pin_changed, HOOK_AFTER,
                 $u_View_pin_change_notify, $u_View,
                 $f_View_pin_changed, $o_View,
                 NULL);
    add_hook_obj(&$h_Workspace_destroy, HOOK_BEFORE,
                 $u_View, $u_View,
                 $f_View_destroy, $o_View,
                 NULL);
    $lo_Workspace_views =
      cons_o($o_View, $lo_Workspace_views);
  }

  $lo_Terminal_views = lmput_o($lo_Terminal_views, $o_View_workspace, $o_View);

  $i_View_cols = $i_Terminal_cols / ($i_column_width + $i_line_meta_width);
  if ($i_View_cols == 0) $i_View_cols = 1;
  $i_View_rows = $i_View_cols * ($i_Terminal_rows - 1);
  if ($i_View_cut_in_workspace < $i_View_rows)
    $i_View_cut_on_screen = $i_View_cut_in_workspace;
  else
    $i_View_cut_on_screen = $i_View_rows;
}

/*
  SYMBOL: $f_View_destroy
    Releases references to the subordinate Workspace of this View, and deletes
    its hook thereinto.
*/
defun($h_View_destroy) {
  $$($($o_View_workspace,$o_Workspace_backing)) {
    del_hook(&$h_Backing_alter, HOOK_AFTER,
             $u_View_Backing_change_notify,
             $o_View);
  }
  $$($o_View_workspace) {
    del_hook(&$h_Workspace_pin_changed, HOOK_AFTER,
             $u_View_pin_change_notify,
             $o_View);
    del_hook(&$h_Workspace_destroy, HOOK_BEFORE,
             $u_View,
             $o_View);
    $lo_Workspace_views = lrm_o($lo_Workspace_views, $o_View);
  }

  $o_View_workspace = NULL;
  $lo_Terminal_views = lmdel_o($lo_Terminal_views, $o_View_workspace);
}

/*
  SYMBOL: $f_View_redraw
    Redraws the entire view, including pins and the echo area.
 */
defun($h_View_redraw) {
  int end = $i_View_cut_in_workspace;
  int begin = end - $i_View_rows;
  if (begin < 0) {
    begin = 0;
    end = $i_View_rows;
  }

  for ($i_View_line_to_paint = begin;
       $i_View_line_to_paint < end;
       ++$i_View_line_to_paint)
    $f_View_paint_line();
  //TODO: pins

  $$($o_View_terminal) {
    $F_Workspace_draw_echo_area(0, $o_View_workspace);
  }
}

/*
  SYMBOL: $f_View_backing_changed
    Updates the view on the screen given the changes which occurred in the
    backing, which must be the current context.
 */
defun($h_View_backing_changed) {
  // Do nothing if not the current view of this View's terminal
  if ($o_View != $($o_View_terminal,$o_Terminal_current_view))
    return;

  // If this was an append and we were at the end, move cut forward
  if ($y_Backing_alteration_was_append &&
      $i_View_cut_in_workspace == $i_Backing_alteration_begin) {
    $i_View_cut_on_screen += $ao_Backing_lines->len - $i_View_cut_in_workspace;
    $i_View_cut_on_screen %= $i_View_rows;
    $i_View_cut_in_workspace = $ao_Backing_lines->len;
  }
  //If the change leaves cut outside the workspace, move it back
  if ($i_View_cut_in_workspace > $ao_Backing_lines->len) {
    $i_View_cut_on_screen -= $i_View_cut_in_workspace - $ao_Backing_lines->len;
    while ($i_View_cut_on_screen < 0)
      $i_View_cut_on_screen += $i_View_rows;
    $i_View_cut_in_workspace = $ao_Backing_lines->len;
  }

  for ($i_View_line_to_paint = $i_Backing_alteration_begin;
       $i_View_line_to_paint < $i_View_cut_in_workspace;
       ++$i_View_line_to_paint)
    $F_View_paint_line(0,$o_View_terminal);

  $i_View_line_to_paint = $i_View_cut_in_workspace - $i_View_rows;
  if ($i_View_line_to_paint < 0)
    $i_View_line_to_paint = $i_View_cut_in_workspace;

  $F_View_paint_line(0,$o_View_terminal);
}

/*
  SYMBOL: $I_View_cut_face
    Face to apply to the first line ahead of cut.
 */
STATIC_INIT_TO($I_View_cut_face, mkface("+X"));

/*
  SYMBOL: $f_View_paint_line
    Paints the line indexed by $i_View_line_to_paint, assuming that the view is
    the current view of its terminal, and that the line to paint is actually
    visible.

  SYMBOL: $i_View_line_to_paint
    The line, as an index into the backing of the view's workspace, to paint in
    a call to $f_View_paint_line.

  SYMBOL: $i_column_width
    The number of characters wide a column of text is.

  SYMBOL: $i_line_meta_width
    The width, in characters, of the line metadata area.
 */
defun($h_View_paint_line) {
  int line_to_paint = $i_View_line_to_paint - $i_View_cut_in_workspace;
  line_to_paint += $i_View_rows /* since the above is always negative */ +
                   $i_View_cut_on_screen;
  line_to_paint %= $i_View_rows;
  int col = line_to_paint / ($i_Terminal_rows-1);
  int row = line_to_paint % ($i_Terminal_rows-1);
  col *= ($i_column_width + $i_line_meta_width);

  qchar line[$i_column_width + $i_line_meta_width+1];
  memset(line, 0, sizeof(line));

  object oline = NULL;
  $$($($o_View_workspace, $o_Workspace_backing)) {
    if ($i_View_line_to_paint >= 0 &&
        $i_View_line_to_paint < $ao_Backing_lines->len)
      oline = $ao_Backing_lines->v[$i_View_line_to_paint];
  }

  if (oline) {
    qmemcpy(line, $(oline, $q_RenderedLine_meta), $i_line_meta_width);
    qstrlcpy(line+$i_line_meta_width,
             $(oline, $q_RenderedLine_body), $i_column_width+1);
  }

  if ($i_View_line_to_paint ==
      $i_View_cut_in_workspace - $i_View_rows ||
      $i_View_line_to_paint == $i_View_cut_in_workspace)
    for (unsigned i = 0; i < lenof(line); ++i)
      line[i] = apply_face($I_View_cut_face, line[i]);

  $$($o_View_terminal) {
    for (unsigned i = 0; i < sizeof(line)/sizeof(qchar)-1; ++i)
      $F_Terminal_putch(0,0, $i_x = col++, $i_y = row, $q_qch = line+i);
  }
}

STATIC_INIT_TO($i_column_width, 80)
STATIC_INIT_TO($i_line_meta_width, 4)

/*
  SYMBOL: $i_View_page_less_lines
    When paging a View, leave this many lines in common with the previous/next
    page.
 */
STATIC_INIT_TO($i_View_page_less_lines, 4)

/*
  SYMBOL: $f_View_scroll $i_View_scroll
    Moves the cut of this View on the workspace, and as necessary on the screen
    to preserve line locations, by $i_View_scroll lines.
 */
defun($h_View_scroll) {
  int old = $i_View_cut_in_workspace;
  $i_View_cut_in_workspace += $i_View_scroll;

  unsigned end = $($($o_View_workspace,
                     $o_Workspace_backing),
                   $ao_Backing_lines)->len;

  if ($i_View_cut_in_workspace < $i_View_rows)
    $i_View_cut_in_workspace = $i_View_rows;
  else if ($i_View_cut_in_workspace > (signed)end)
    $i_View_cut_in_workspace = end;

  $i_View_cut_on_screen += $i_View_cut_in_workspace - old;
  while ($i_View_cut_on_screen < 0)
    $i_View_cut_on_screen += $i_View_rows;
  $i_View_cut_on_screen %= $i_View_rows;

  $m_redraw();
}

/*
  SYMBOL: $f_View_page_up
    Scrolls the view one page up, except for $i_View_page_less_lines.
 */
defun($h_View_page_up) {
  $M_scroll(0,0, $i_View_scroll = -($i_View_rows - $i_View_page_less_lines));
}

/*
  SYMBOL: $f_View_page_down
    Scrolls the view one page down, except for $i_View_page_less_lines.
 */
defun($h_View_page_down) {
  $M_scroll(0,0, $i_View_scroll = $i_View_rows - $i_View_page_less_lines);
}

/*
  SYMBOL: $f_View_home
    Moves the scroll of this View back to the top.
 */
defun($h_View_home) {
  $M_scroll(0,0, $i_View_scroll = $i_View_rows - $i_View_cut_in_workspace);
}

/*
  SYMBOL: $f_View_end
    Moves the scroll of this View to the end.
 */
defun($h_View_end) {
  unsigned end = $($($o_View_workspace, $o_Workspace_backing),
                   $ao_Backing_lines)->len;
  $M_scroll(0,0, $i_View_scroll = end - $i_View_cut_in_workspace);
}

/*
  SYMBOL: $f_View_scroll_up $I_LastCommand_view_scroll_up
    Scrolls the view one line up, accelerating.
 */
defun($h_View_scroll_up) {
  $i_View_scroll = -accelerate(&$I_LastCommand_view_scroll_up);
  $m_scroll();
}

/*
  SYMBOL: $f_View_scroll_down $I_LastCommand_view_scroll_down
    Scrolls the View one line down, accelerating.
 */
defun($h_View_scroll_down) {
  $i_View_scroll = accelerate(&$I_LastCommand_view_scroll_down);
  $m_scroll();
}

/*
  SYMBOL: $lp_View_keymap
    The basic key commands supported by the View class.
 */
class_keymap($c_View, $lp_View_keymap, $llp_View_keymap)
ATSINIT {
  bind_char($lp_View_keymap, $u_meta, L'e', $v_end_meta,
            $m_page_up);
  bind_char($lp_View_keymap, $u_meta, L'E', $v_end_meta,
            $m_home);
  bind_char($lp_View_keymap, $u_meta, L'r', $v_end_meta,
            $m_page_down);
  bind_char($lp_View_keymap, $u_meta, L'R', $v_end_meta,
            $m_end);
  bind_kp($lp_View_keymap, $u_ground, KEY_DOWN, NULL,
          $m_scroll_down);
  bind_kp($lp_View_keymap, $u_ground, KEY_SF, NULL,
          $m_scroll_down);
  bind_kp($lp_View_keymap, $u_ground, KEY_UP, NULL,
          $m_scroll_up);
  bind_kp($lp_View_keymap, $u_ground, KEY_SR, NULL,
          $m_scroll_up);
}
