#! /bin/sh

# Check usage
if test 0 = "$#"; then
    cat <<EOF
Usage: $0 <suite|module> ...
  Boostraps the build system, including the listed suites and/or modules.
  If you just want to get started quickly, you probably want
    ./regen.sh std
  to build the recommended set of modules, or
    ./regen.sh all
  to include everything.

  Each module is a directory under src/, and contains a README explaining
  its purpose, the general weight of building that module in, as well as the
  weight of actually using it.

  Suites, and what they contain, are listed under src/suites/

  Including any module implictly includes the modules it depends on.

  If you REALLY want to build Soliloquy with none of the optional modules, run
    ./regen.sh nightmare
EOF
    exit 1
fi

cd `dirname $0`

# Reinit the makefile
cp src/Makefile.am.head src/Makefile.am

# Clean the modules list if already present
rm -Rf .modules
mkdir -p .modules

# Get list of suites
. src/suites

require() {
    module=$1
    # Do nothing if already handled
    if test -f ".modules/$module"; then
        return
    fi

    # Is it a suite?
    if test -f "src/suites/$module"; then
        . "src/suites/$module"
    # Else, is it a module?
    elif test -d "src/$module"; then
        test -f "src/$module/modifno" && . "src/$module/modinfo"
        printf "%s " `ls "src/$module/"*.c | sed 's:src/::'` >>src/Makefile.am
    else
        printf "Module or suite not found: %s\n" "$module" >&2
        exit 1
    fi

    # Remember that we already handled this module
    touch ".modules/$module"
}

# List source files in Soliloquy core
printf 'sol_SOURCES=' >>src/Makefile.am
printf '%s ' `ls src/*.c | sed 's:src/::'` >>src/Makefile.am

# Include modules/sources
for mod in $*; do
    require $mod || exit $?
done

echo >>src/Makefile.am
# Compile silc files as needed
find src -name '*.c' -exec printf '%s\n' '{}' \; | sed 's:src/::g' | (while read name; do
    name=`echo $name | sed 's/.c$//'`
    printf "$name.slc: $name.c silc\$(EXEEXT) dynar.plt list.plt | classes\n"
    printf "\\t@echo '  SILC   $name.slc'\n"
    printf "\\t@./silc\$(EXEEXT) $name\n"
    printf "$name.o: $name.slc\n"
done) >>src/Makefile.am

rm -Rf .modules

autoreconf -i
