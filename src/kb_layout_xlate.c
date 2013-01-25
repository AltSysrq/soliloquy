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
#include "kb_layout_xlate.slc"

/*
  TITLE: Keyboard Layout Inverse Translation

  OVERVIEW: Most simple meta keybindings are chosen based on physical location
    instead of mnemonics. In most editors, physical location keybindings become
    less useful and potentially awkward for users of keyboard layouts which
    differ from those who set the keybindings up.

    This file defines advice on $f_Terminal_getch() which transparently
    converts the 30 "primary" keys (Q..P, A..:, Z../ on US QWERTY) from their
    real form to the character values they would have on US QWERTY, when
    $v_Terminal_key_mode is in $lv_Terminal_kb_xlate_modes.

    Keyboard layout is per-terminal, and defaults to a value defined at compile
    time (see below).
*/

/*
  The most common Latin keyboard layouts I could find are below. Adding your
  own should be relatively straight-forward. One-handed layouts were excluded
  since the default keybindings are not that useful for them anyway.
 */
ATSTART(initalise_keyboard_layouts, STATIC_INITIALISATION_PRIORITY-1) {
  // US QWERTY
  $$w_layout_qwerty_us    = L"qwertyuiopasdfghjkl;zxcvbnm,./"
                             "QWERTYUIOPASDFGHJKL:ZXCVBNM<>?";
  // DE Neo2
  $$w_layout_neo2         = L"xvlcwkhgfquiaeosnrtdüöäpzbm,.j"
                             "XVLCWKHGFQUIAEOSNRTDÜÖÄPZBM–•J";
  // DE QWERTZ
  $$w_layout_qwertz_de    = L"qwertzuiopasdfghjklöyxcvbnm,.-"
                             "QWERTZUIOPASDFGHJKLÖXYCVBNM;:_";
  // QWERTZ, Swiss French
  $$w_layout_qwertz_ch    = L"qwertzuiopasdfghjlkéyxcvbnm,.-"
                             "QWERTZUIOPASDFGHJKLÉYXCVBNM;:_";
  // Slovene QWERTZ
  $$w_layout_qwertz_sl    = L"qwertzuiopasdfghjklčyxcvbnm,.-"
                             "QWERTZUIOPASDFGHJKLČYXCVBNM;:_";
  // HU QWERTZ
  $$w_layout_qwertz_hu    = L"qwertzuiopasdfghjkléyxcvbnm,.-"
                             "QWERTZUIOPASDFGHJKLÉYXCVBNM?:_";
  // FR AZERTY
  $$w_layout_azerty_fr    = L"azertyuiopqsdfghjklmwxcvbn,;:!"
                             "AZERTYUIOPQSDFGHJKLMWXCVBN?./§";
  // US Dvorak Classic
  $$w_layout_dvorak_c     = L"/,.pyfgcrlaoeuidhtns;qjkxbmwvz"
                             "?<>PYFGCRLAOEUIDHTNS:QJKXBMWVZ";
  // US Dvorak Modern
  $$w_layoud_dvorak_m     = L"',.pyfgcrlaoeuidhtns;qjkxbmwvz"
                            "\"<>PYFGCRLAOEUIDHTNS:QJKXBMWVZ";
  // SV Svorak
  $$w_layout_svorak       = L"åäöpyfgcrlaoeuidhtns.qjkxbmwvz"
                             "ÅÄÖPYFGCRLAOEUIDHTNS:QJKXBMWVZ";
  // US Colemak
  $$w_layout_colemak      = L"qwfpgjluy;arstdhneiozxcvbkm,./"
                             "QWFPGJLUY:ARSTDHNEIOZXCVBKM<>?";
  // UK Maltron (US Maltron is a singular transform we can't really handle)
  $$w_layout_maltron      = L"qpycbvmuzlanisfdthor;/jg,.wk-x"
                             "QPYCBVMUZLANISFDTHOR:?JG<>WK_X";
  // Turkish-F
  $$w_layout_turkish_f    = L"fgǧıodrnhpuieaütkmlyjövcçzsb.,"
                             "FGǦIODRNHPUİEAÜTKMLYJÖVCÇZSB:;";
}

// Some aliases people will expect
STATIC_INIT_TO($$w_layout_dvorak, $$w_layout_dvorak_m)
STATIC_INIT_TO($$w_layout_neo, $$w_layout_neo2)
STATIC_INIT_TO($$w_layout_qwertz, $$w_layout_qwertz_de)
STATIC_INIT_TO($$w_layout_azerty, $$w_layout_azerty_fr)
STATIC_INIT_TO($$w_layout_qwerty, $$w_layout_qwerty_us)

/*
  The default keyboard layout is determined via the preprocessor symbol
  KB_LAYOUT, or defaults to qwerty_us if that is not given.

  You can easily set this at compile time by specifying the definition in the
  CFLAGS when you call make (eg, `make 'CFLAGS=-DKB_LAYOUT=neo2`).
*/
#ifdef KB_LAYOUT
#define DEFAULT_KB_LAYOUT _GLUE(_GLUE($$,w_layout_),KB_LAYOUT)
#else
#define DEFAULT_KB_LAYOUT $$w_layout_qwerty_us
#endif

/*
  SYMBOL: $w_Terminal_keyboard_layout
    The current layout of the 30 primary keys for this Terminal (the root
    object holds the default layout). This is a string exactly 60 characters
    long; the first 30 are the unshifted primary keys, which should be
    transformed to "qwertyuiopasdfghjkl;zxcvbnm,./"; the next 30 correspond to
    the shifted keys, which transform to the shifted equivalents on US QWERTY.
 */
STATIC_INIT_TO($w_Terminal_keyboard_layout, DEFAULT_KB_LAYOUT)

/*
  SYMBOL: $lv_Terminal_kb_xlate_modes
    A list of key modes ($v_Terminal_key_mode) in which keyboard layout
    translation applies. By default, it is only the base meta mode, $u_meta.
 */
STATIC_INIT_TO($lv_Terminal_kb_xlate_modes,
               cons_v($u_meta, $lv_Terminal_kb_xlate_modes))
#undef DEFAULT_KB_LAYOUT

/*
  SYMBOL: $u_kb_xlate
    Identifies the keyboard translation hook on $h_Terminal_getch.
 */
advise_id_before($u_kb_xlate, $h_Terminal_getch) {
  // Do nothing if not a character
  if ((1<<31) & $x_Terminal_input_value) return;

  if (find_v($lv_Terminal_kb_xlate_modes, $v_Terminal_key_mode)) {
    wstring layout = $w_Terminal_keyboard_layout;
    wstring target = $$w_layout_qwerty_us;
    while (*layout && $x_Terminal_input_value != *layout)
      ++layout, ++target;

    if (*layout)
      // Found a match
      $x_Terminal_input_value = *target;
  }
}
