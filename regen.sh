#! /bin/sh

# Reinit the makefile
cp src/Makefile.am.head src/Makefile.am

# List source files
printf 'sol_SOURCES=' >>src/Makefile.am
find src -name '*.c' -printf '%P ' >>src/Makefile.am
echo >>src/Makefile.am
# Compile silc files as needed
find src -name '*.c' -printf '%P\n' | (while read name; do
    name=`echo $name | sed 's/.c$//'`
    printf "$name.slc: $name.c silc dynar.plt list.plt | classes\n"
    printf "\\t@echo '  SILC   $name.slc'\n"
    printf "\\t@./silc $name\n"
    printf "$name.o: $name.slc\n"
done) >>src/Makefile.am

autoreconf -i
