#! /usr/bin/env tclsh
# Generates the symbols.def file, which declares concrete values for all
# symbols, creates their metadata, and binds $name to their source-code name.

set symbols {}
foreach source [exec find . -name *.c] {
  set in [open $source r]
  set contents [read $in]
  close $in

  set symbols [concat $symbols [regexp -all -inline {\$[a-zA-Z0-9_]+} $contents]]
}

lappend symbols {$name}
set symbols [lsort -unique $symbols]

set out [open "symbols.def" w]
puts $out {
/* Autogenerated by silc_symbols.tcl. Do not edit! */
/* To be #included by common.c */
}

foreach sym $symbols {
  puts $out "static struct sytab __$sym;"
  puts $out "static sytab _$sym;"
  puts $out "symbol $sym;"
}

puts $out \
    "static void init_symbols(void) ATSTART \{"
puts $out \
    "syval nameval;"
puts $out \
    "nameval.type = Syval_String;"

foreach sym $symbols {
  puts $out "_$sym = &__$sym;"
  puts $out "$sym = &_$sym;"
}

foreach sym $symbols {
  set name "\"$sym\""
  puts $out "sytab_init(_$sym);"
  puts $out "nameval.value.as_string = $name;"
  puts $out "sytab_put(_$sym, \$name, nameval);"
}

puts $out "}"

close $out
