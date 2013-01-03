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
    echo "$name.slc: $name.c silc dynar.plt list.plt"
    echo "\\t@echo '  SILC   $name.slc'"
    echo "\\t@./silc $name"
    echo "$name.o: $name.slc"
done) >>src/Makefile.am

autoreconf -i
