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

/*
  TITLE: Translates formatted characters into ncurses cchar_ts.
  OVERVIEW: This file is responsible for the handling the rendition of
    characters on the terminal. You will want to change things here if your
    terminal interprets things unusually (ie, blinking) or if you want to
    change the way formatting is converted to what your terminal understands.
    If you just want to change the formatting or colours which are used, you
    probably want to edit faces_conf.c.

  SEE ALSO: qstring.h for attribute and colour definitions
*/

#include "colour_attr_xlate.slc"
#ifndef _XOPEN_SORUCE_EXTENDED
#define _XOPEN_SORUCE_EXTENDED 1
#endif
#include <ncursesw/curses.h>

#ifdef HAVE_USE_DEFAULT_COLORS
/*
  Define one of these three. OPAQUE means that your entire terminal is solidly
  shaded with background colour 0. This is the most conservative setting, and
  is the only one available on non-ncurses implementations of curses.

  TRANSPARENT remaps all usage a non-bright background black to the "default"
  colour. On real terminals, this generally is no different from
  OPAQUE. However, in emulators like urxvt, the default background may be
  transparent (ie, shows your desktop wallpaper); thus, all cells with
  non-coloured backgrounds will have a transparent background, rendering the
  text on top of your wallpaper (or whatever it is your favourite terminal
  does). If you use this, you will probably need to define
  BLINK_ACTUALLY_BLINKS (see below) since the default background will actually
  be used.

  TRANSLUSCENT is a hybrid of the two. It behaves like OPAQUE for all
  characters other than the NUL character with a black background, for which it
  behaves like TRANSPARENT. Thus, the empty parts of your terminal allow your
  wallpaper to show through, while areas with text have a solid background and
  are thus easy to read. Unlike with TRANSPARENT, you probably won't have to
  worry about BLINK_ACTUALLY_BLINKS.

  TRANSLUSCENT_SOUP is like TRANSLUSCENT, except that space characters also use
  the default background.
 */
//#define BG_OPAQUE
//#define BG_TRANSPARENT
#define BG_TRANSLUSCENT
//#define BG_TRANSLUSCENT_SOUP
#else
//Without use_default_colors(), we can't do anything other than opaque.
#define BG_OPAQUE
#endif

/*
  Define this if your terminal actually interprets the "blink" rendition as
  such, even for non-default background colours. This will prevent Soliloquy
  from trying to use to render bright, non-bold colours.
 */
//#define BLINK_ACTUALLY_BLINKS

/*
  Define zero, one, or both of these based on whether your terminal makes
  bright characters bold, and bold characters bright, respectively. An example
  of the former is the Emacs terminal-emulator; of the latter is urxvt.

  Bold-implies-bright is a mixed blessing. On the down side, it makes it
  impossible to get a non-bright foreground without a bright background. On the
  other hand, it's also the only way to get both a bright foreground and
  background.
 */
//#define BRIGHT_IMPLIES_BOLD
#define BOLD_IMPLIES_BRIGHT

/*
  If you can configure your terminal so that a "bright black" background is
  fully black, defining this will allow you to have non-bright bold characters
  on a black background even if BOLD_IMPLIES_BRIGHT. If you use a transparent
  terminal and the BG_TRANSPARENT setting, you will need to make the bright
  black background transparent instead.

  If defined, any real attempt to use a bright black background will be
  replaced with a dark red background (see BRIGHT_BLACK_BG_REPLACEMENT).
*/
//#define BRIGHT_BLACK_BG_IS_BLACK

/*
  If BRIGHT_BLACK_BG_IS_BLACK is defined, this colour is used to replace
  otherwise legitimate uses of bright black as a background. The default, 0x8,
  is dark red.
*/
#define BRIGHT_BLACK_BG_REPLACEMENT DARK_RED

/*
  Like BRIGHT_BLACK_BG_IS_BLACK, but affects bright magenta instead, since that
  is generally the colour people least want as a background. The same caveats
  for this setting as for BRIGHT_BLACK_BG_IS_BLACK apply.

  If defined, any real attempt to use a bright magenta background will be
  replaced with a bright blue background (see BRIGHT_MAGENTA_BG_REPLACEMENT).
*/
//#define BRIGHT_MAGENTA_BG_IS_BLACK

/*
  If BRIGHT_MAGENTA_BG_IS_BLACK is defined, this colour is used to replace
  otherwise legitimate uses of bright magenta as a background. The default,
  0x3, is bright blue.
*/
#define BRIGHT_MAGENTA_BG_REPLACEMENT BRIGHT_BLUE

member_of_domain($$d_Terminal,$d_Terminal)

/*
  SYMBOL: $i_Terminal_num_colours $i_Terminal_num_colour_pairs
    These reflect the values of the curses variables COLORS and COLOR_PAIRS at
    the time the terminal was created. See color(3ncurses).

  SYMBOL: $y_Terminal_has_colour
    Set to true if the terminal supports colour.

  SYMBOL: $y_Terminal_can_change_colour
    Set to true if the terminal can change its colour on-the-fly.

  SYMBOL: $f_Terminal_do_colour_changes
    Called at terminal initialisation time if the terminal supports colour
    changes. There is no default implementation.

*/

advise_after($h_Terminal) {
  start_color();
#ifndef BG_OPAQUE
  use_default_colors();
#endif

  $i_Terminal_num_colours = COLORS;
  $i_Terminal_num_colour_pairs = COLOR_PAIRS;
  $y_Terminal_has_colour = has_colors();
  $y_Terminal_can_change_colour = can_change_color();
  if ($y_Terminal_can_change_colour)
    $f_Terminal_do_colour_changes();

  // Initialise colour pairs
  if ($y_Terminal_has_colour) {
    const int ncurses_colours[8] = {
#ifndef BG_TRANSPARENT
      COLOR_BLACK,
#else
      -1, // The "default" background, ie, transparent
#endif
      COLOR_BLUE, COLOR_GREEN, COLOR_CYAN,
      COLOR_RED, COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE,
    };

    // Initialise the first COLOUR_PAIRS-1 colours (or all 64 if they
    // exist). Those that don't fit will get wrapped around later. (This will
    // generally only affect white-on-white, making it black-on-black.)
    // Just in case, anyway, make sure to make the foreground the inner loop,
    // since colour-on-black is far more important than anything-on-colour.
    //
    // The colour pair schema here is
    //   PAIR = 1 + (FG + (BG<<3) - 1)%(COLOR_PAIRS-1)
    // where FG and BG are both 3-bit integers indicating red, green, and blue
    // from most to least significant bits (and foreground being additive as
    // normal, instead of inverted as qchars use).
    unsigned pair = 1;
    for (unsigned bg = 0; bg < 8 && pair < $i_Terminal_num_colour_pairs; ++bg)
      for (unsigned fg = 0; fg < 8 && pair < $i_Terminal_num_colour_pairs; ++fg)
        init_pair(pair++, ncurses_colours[bg], ncurses_colours[fg]);
  }
}

defun($h_translate_qchar_to_ncurses) {
  // Start by dissecting the qchar
  qchar ch = *$q_qch;
  wchar_t character = qchrtowchr(ch);
  unsigned fg = (ch & QC_FG) >> QC_FG_SHIFT;
  unsigned bg = (ch & QC_BG) >> QC_BG_SHIFT;
  bool underline = !!(ch & QC_ULIN);
  bool bolded = !!(ch & QC_BOLD);
  bool reverse_video = !!(ch & QC_RVID);
  bool blink = false;
  /* Don't support italics yet */

  /* If we're using substitute bright backgrounds, handle those now. */
#ifdef BRIGHT_BLACK_BG_IS_BLACK
  if (bg == BRIGHT_BLACK)
    bg = BRIGHT_BLACK_BG_REPLACEMENT;
#endif

#ifdef BRIGHT_MAGENTA_BG_IS_BLACK
  if (bg == BRIGHT_MAGENTA)
    bg = BRIGHT_MAGENTA_BG_REPLACEMENT;
#endif

  // Separate into colours proper and brightnesses
  bool brightfg = fg & 1;
  fg >>= 1;
  bool brightbg = bg & 1;
  bg >>= 1;
  // Invert foreground to normal additive format
  fg ^= 0x7;

  /* We'll be using hardware reverse video to get things like bright
   * foregrounds and such, so handle logical reverse video manually.
   */
  if (reverse_video) {
    fg ^= bg;
    bg ^= fg;
    fg ^= bg;
    brightfg ^= brightbg;
    brightbg ^= brightfg;
    brightfg ^= brightbg;
    reverse_video = false;
  }

  /* Underline generally doesn't affect anything else, so it can just be passed
   * through.
   */

#define SETRV \
  do { fg ^= bg; bg ^= fg; fg ^= bg; reverse_video = true; } while (0)
  /* Getting colours, brightness, and boldness to play well together is
   * surprisingly difficult and varies somewhat by terminal (see definitions
   * earlier in this file). There are eight possible combinations:
   *
   * dim fg, dim bg, nonbold:
   * This is the default and requires no special handling.
   *
   * bright fg, dim bg, nonbold:
   * If not BLINK_ACTUALLY_BLINKS: Swap fg and bg colours, set blink, set
   *   reverse video.
   * If BOLD_IMPLIES_BRIGHT, set bold. (We get bright fg, dim bg, bold.)
   * Otherwise, we get dim fg, dim bg, nonbold.
   *
   * dim fg, bright bg, nonbold:
   * If not BLINK_ACTUALLY_BLINKS: Set blink.
   * If BOLD_IMPLIES_BRIGHT: Swap fg and bg, set bold, set reverse video.
   *   (This gives us dim fg, bright bg, bold in most cases.)
   * Otherwise, we get dim fg, dim bg, nonbold.
   *
   * bright fg, bright bg, nonbold:
   * If BOLD_IMPLIES_BRIGHT and not BLINK_ACTUALLY_BLINKS: Set blink, set
   *   bold. (We get bright fg, bright bg, bold.)
   * If BOLD_IMPLIES_BRIGHT: Set bold. (We get bright fg, dim bg, bold)
   * If not BLINK_ACTUALLY_BLINKS: Swap fg and bg colours, set reverse video,
   *   set blink. (We get bright fg, dim bg, nonbold.)
   * Otherwise, we get dim fg, dim bg, nonbold.
   *
   * dim fg, dim bg, bold:
   * If not BOLD_IMPLIES_BRIGHT: set bold.
   * If BRIGHT_BLACK_BG_IS_BLACK: Set bg to black, swap fg and bg colours, set
   *   reverse video, set bold.
   * If BRIGHT_MAGENTA_BG_IS_BLACK: Set bg to magenta, swap fg and bg colours,
   *   set reverse video, set bold.
   * Otherwise, set bold; we get bright bg, dim bg, bold.
   *
   * bright fg, dim bg, bold:
   * If BOLD_IMPLIES_BRIGHT: Set bold.
   * If BRIGHT_IMPLIES_BOLD and not BLINK_ACTUALLY_BLINKS:
   *   Swap fg and bg colours, set blink, set reverse video.
   * Otherwise, set bold. We get dim fg, dim bg, bold.
   *
   * dim fg, bright bg, bold:
   * If not BOLD_IMPLIES_BRIGHT and not BLINK_ACTUALLY_BLINKS:
   *   Set bold, set blink.
   * If BOLD_IMPLIES_BRIGHT: Swap bg and fg, set reverse video, set bold.
   * If not BLINK_ACTUALLY_BLINKS: Set bold, set blink. (We get bright fg,
   * bright bg, bold).
   * Otherwise: Set bold. (We get dim fg, dim bg, bold.)
   *
   * bright fg, bright bg, bold:
   * If BOLD_IMPLIES_BRIGHT and not BLINK_ACTUALLY_BLINKS:
   *   Set bold, set blink.
   * If BOLD_IMPLIES_BRIGHT: Set bold. (We get bright fg, dim bg, bold.)
   * If not BLINK_ACTUALLY_BLINKS: Set bold, set blink. (We get dim fg, bright
   *   bg, bold).
   * Otherwise, set bold. (We get dim fg, dim bg, bold.)
   */
  bool bold = false;
  switch ((bolded<<2) | (brightbg << 1) | brightfg) {
  case 0: // dim, dim, norm
    break;

  case 1: // bright, dim, norm
#if !defined(BLINK_ACTUALLY_BLINKS)
    SETRV;
    blink = true;
#elif defined(BOLD_IMPLIES_BRIGHT)
    bold = true;
#else
    /* Do nothing */
#endif
    break;

  case 2: // dim, bright, norm
#if !defined(BLINK_ACTUALLY_BLINKS)
    blink = true;
#elif defined(BOLD_IMPLIES_BRIGHT)
    SETRV;
    bold = true;
#else
    /* Do nothing */
#endif
    break;

  case 3: // bright, bright, norm
#if defined(BOLD_IMPLIES_BRIGHT) && !defined(BLINK_ACTUALLY_BLINKS)
    blink = true;
    bold = true;
#elif defined(BOLD_IMPLIES_BRIGHT)
    bold = true;
#else
    /* Do nothing */
#endif
    break;

  case 4: // dim, dim, bold
#if !defined(BOLD_IMPLIES_BRIGHT)
    bold = true;
#elif defined(BRIGHT_BLACK_BG_IS_BLACK)
    bg = BRIGHT_BLACK >> 1;
    SETRV;
    bold = true;
#elif defined(BRIGHT_MAGENTA_BG_IS_BLACK)
    bg = BRIGHT_MAGENTA >> 1;
    SETRV;
    bold = true;
#else
    bold = true;
#endif
    break;

  case 5: // bright, dim, bold
#if defined(BOLD_IMPLIES_BRIGHT)
    bold = true;
#elif defined(BRIGHT_IMPLIES_BOLD) && !defined(BLINK_ACTUALLY_BLINKS)
    SETRV;
    blink = true;
#else
    bold = true;
#endif
    break;

  case 6: // dim, bright, bold
#if !defined(BOLD_IMPLIES_BRIGHT) && !defined(BLINK_ACTUALLY_BLINKS)
    bold = true;
    blink = true;
#elif defined(BOLD_IMPLIES_BRIGHT)
    SETRV;
    bold = true;
#elif !defined(BLINK_ACTUALLY_BLINKS)
    bold = true;
    blink = true;
#else
    bold = true;
#endif
    break;

  case 7: // bright, bright, bold
#if defined(BOLD_IMPLIES_BRIGHT) && !defined(BLINK_ACTUALLY_BLINKS)
    bold = true;
    blink = true;
#elif defined(BOLD_IMPLIES_BRIGHT)
    bold = true;
#elif !defined(BLINK_ACTUALLY_BLINKS)
    blink = true;
    bold = true;
#else
    bold = true;
#endif
    break;
  }

  unsigned colour_pair =
    1 + ((fg << 3) + bg - 1) % ($i_Terminal_num_colour_pairs-1);

  // NUL is handled specially
  if (!character) {
    character = L' ';
#ifndef BG_OPAQUE
    if ((reverse_video? fg : bg) == 0)
      //Transparent
      colour_pair = 0;
#endif
  }
#ifdef BG_TRANSLUSCENT_SOUP
  else if (character == L' ') {
    if ((reverse_video? fg : bg) == 0)
      colour_pair = 0;
  }
#endif

  cchar_t* wch = $p_wch;
  memset(wch, 0, sizeof(cchar_t));
  wch->chars[0] = character;
  wch->attr = COLOR_PAIR(colour_pair);
  if (bold)
    wch->attr |= A_BOLD;
  if (underline)
    wch->attr |= A_UNDERLINE;
  if (reverse_video)
    wch->attr |= A_REVERSE;
  if (blink)
    wch->attr |= A_BLINK;
}
