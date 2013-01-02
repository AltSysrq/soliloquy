#! /bin/sh

# Reinit the makefile
cp src/Makefile.am.head src/Makefile.am

# List source files
echo -n 'sol_SOURCES=' >>src/Makefile.am
find src -name '*.c' -printf '%P ' >>src/Makefile.am
echo >>src/Makefile.am
# Compile silc files as needed
find src -name '*.c' -printf '%P\n' | (while read name; do
    name=`echo $name | sed 's/.c$//'`
    echo "$name.slc: $name.c silc dynar.m4 list.m4 implicit_symbols"
    echo "\\t@echo '  SILC   $name.slc'"
    echo "\\t@./silc $name"
    echo "$name.o: $name.slc"
done) >>src/Makefile.am

# Global symbols
echo "common.o: symbols.def" >>src/Makefile.am
echo "symbols.def: silc dynar.m4 list.m4 implicit_symbols \$(sol_SOURCES)" >>src/Makefile.am
echo "\\t@echo '  SILC   GLOBALS'" >>src/Makefile.am
echo "\\t@./silc" >>src/Makefile.am

autoreconf -i
