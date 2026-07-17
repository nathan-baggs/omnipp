#!/bin/sh -e
# This is used for renaming symbols for the fat runtime, don't call directly
# TODO: make this a lot less fragile!
cleanup () {
    rm -f ${SYMSFILE} ${KEEPSYMS}
}
NM="${NM:-nm}"
OBJCOPY="${OBJCOPY:-objcopy}"
PREFIX=$1
KEEPSYMS_IN=$2
shift 2
# $@ contains the actual build command
OUT=$(echo "$@" | rev | cut -d ' ' -f 2- | rev | sed 's/.* -o \(.*\.o\).*/\1/')
trap cleanup INT QUIT EXIT
SYMSFILE=$(mktemp -p /tmp ${PREFIX}_rename.syms.XXXXX)
KEEPSYMS=$(mktemp -p /tmp keep.syms.XXXXX)
# find the libc used by gcc
LIBC_SO=$("$@" --print-file-name=libc.so.6)
NM_DYN="-D"
NM_FLAG="-f"
if [ `uname` = "FreeBSD" ]; then
    # for freebsd, we will specify the name,
    # we will leave it work as is in linux
    LIBC_SO=/lib/libc.so.7
    # also, in BSD, the nm flag -F corresponds to the -f flag in linux.
    NM_FLAG="-F"
fi
# musl doesn't have libc.so.6, try musl's shared library naming
if [ ! -f "$LIBC_SO" ]; then
    LIBC_SO=$("$@" --print-file-name=libc.musl-$(uname -m).so.1)
fi
# if still not found, try static libc.a (no -D flag for static archives)
if [ ! -f "$LIBC_SO" ]; then
    LIBC_SO=$("$@" --print-file-name=libc.a)
    if [ -f "$LIBC_SO" ]; then
        NM_DYN=""
    fi
fi
cp ${KEEPSYMS_IN} ${KEEPSYMS}
# get all symbols from libc and turn them into patterns
if [ -n "$LIBC_SO" ] && [ -f "$LIBC_SO" ]; then
    ${NM} ${NM_FLAG} posix -g ${NM_DYN} ${LIBC_SO} | sed 's/\([^ @]*\).*/^\1$/' >> ${KEEPSYMS}
else
    # Hard fallback for musl/cross-compilers where libc isn't found
    # Prevent standard C library symbols from being renamed
    cat >> ${KEEPSYMS} << 'EOF'
^memset$
^memcpy$
^memmove$
^memcmp$
^strlen$
^malloc$
^calloc$
^free$
^realloc$
^abort$
^exit$
^printf$
^fprintf$
^sprintf$
^snprintf$
^puts$
^fputs$
^fwrite$
^fread$
^fopen$
^fclose$
^stderr$
^stdout$
^stdin$
EOF
fi
# build the object
"$@"
# rename the symbols in the object
${NM} ${NM_FLAG} posix -g ${OUT} | cut -f1 -d' ' | grep -v -f ${KEEPSYMS} | sed -e "s/\(.*\)/\1\ ${PREFIX}_\1/" >> ${SYMSFILE}
if test -s ${SYMSFILE}
then
    ${OBJCOPY} --redefine-syms=${SYMSFILE} ${OUT}
fi
