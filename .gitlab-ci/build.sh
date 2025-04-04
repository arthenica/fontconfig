#! /bin/bash

set -ex
set -o pipefail

cidir=$(dirname $0)
[ -f ${cidir}/fcenv ] && . ${cidir}/fcenv

case "$OSTYPE" in
    msys) MyPWD=$(pwd -W) ;;
    *BSD) PATH=$PATH:/usr/local/bin ;&
    *) MyPWD=$(pwd) ;;
esac
enable=()
disable=()
distcheck=0
enable_install=1
disable_check=0
cross=0
buildsys="autotools"
type="both"
arch=""
buildopt=()
SRCDIR=$MyPWD
export MAKE=${MAKE:-make}
export BUILD_ID=${BUILD_ID:-fontconfig-$$}
export PREFIX=${PREFIX:-$MyPWD/prefix}
export BUILDDIR=${BUILDDIR:-$MyPWD/build}

while getopts a:cCe:d:hIs:t:X: OPT
do
    case $OPT in
        'a') arch=$OPTARG ;;
        'c') distcheck=1 ;;
        'C') disable_check=1 ;;
        'e') enable+=($OPTARG) ;;
        'd') disable+=($OPTARG) ;;
        'I') enable_install=0 ;;
        's') buildsys=$OPTARG ;;
        't') type=$OPTARG ;;
        'X') backend=$OPTARG ;;
        'h')
            echo "Usage: $0 [-a ARCH] [-c] [-e OPT] [-d OPT] [-I] [-s BUILDSYS] [-t BUILDTYPE] [-X XMLBACKEND]"
            exit 1
            ;;
    esac
done
case x"$FC_BUILD_PLATFORM" in
    'xmingw') cross=1 ;;
    'xandroid') cross=1 ;;
    *) cross=0 ;;
esac

env
r=0

if [ x"$buildsys" == "xautotools" ]; then
    for i in "${enable[@]}"; do
        buildopt+=(--enable-$i)
    done
    for i in "${disable[@]}"; do
        buildopt+=(--disable-$i)
    done
    case x"$backend" in
        'xexpat')
            buildopt+=(--disable-libxml2)
            ;;
        'xlibxml2')
            buildopt+=(--enable-libxml2)
            ;;
    esac
    case x"$type" in
        'xshared')
            buildopt+=(--enable-shared)
            buildopt+=(--disable-static)
            ;;
        'xstatic')
            buildopt+=(--disable-shared)
            buildopt+=(--enable-static)
            ;;
        'both')
            buildopt+=(--enable-shared)
            buildopt+=(--enable-static)
            ;;
    esac
    if [ $cross -eq 1 -a -n "$arch" ]; then
        buildopt+=(--host=$arch)
        if [ ! -f .gitlab-ci/${FC_DISTRO_NAME}-cross.sh ]; then
            echo "No ${FC_DISTRO_NAME}-cross.sh available"
            exit 1
        fi
        . .gitlab-ci/${FC_DISTRO_NAME}-cross.sh
    fi
    rm -rf "$BUILDDIR" "$PREFIX" || :
    mkdir "$BUILDDIR" "$PREFIX"
    cd "$BUILDDIR"
    ../autogen.sh --prefix="$PREFIX" --disable-cache-build ${buildopt[*]} 2>&1 | tee /tmp/fc-build.log || r=$?
    $MAKE V=1 2>&1 | tee -a /tmp/fc-build.log || r=$?
    if [ $disable_check -eq 0 ]; then
        $MAKE check V=1 2>&1 | tee -a /tmp/fc-build.log || r=$?
    fi
    if [ $enable_install -eq 1 ]; then
        $MAKE install V=1 2>&1 | tee -a /tmp/fc-build.log || r=$?
    fi
    if [ $distcheck -eq 1 ]; then
        $MAKE distcheck V=1 2>&1 | tee -a /tmp/fc-build.log || r=$?
    fi
elif [ x"$buildsys" == "xmeson" ]; then
    pip install meson
#   tomli not required for Python >= 3.11
    pip install tomli
    for i in "${enable[@]}"; do
        buildopt+=(-D$i=enabled)

        # Update bindgen on Fontations builds to improve support for constants in fcint.h
        if [[ "$i" == "fontations" ]]; then
            cargo install bindgen-cli
            # Prepend the cargo bin directory to PATH
            if [[ -d "$HOME/.cargo/bin" ]]; then
                export PATH="$HOME/.cargo/bin:$PATH"
                echo "Cargo bin directory added to PATH."
            else
                echo "Cargo bin directory not found."
            fi
        fi
    done
    for i in "${disable[@]}"; do
        buildopt+=(-D$i=disabled)
    done
    case x"$backend" in
        'xexpat')
            buildopt+=(-Dxml-backend=expat)
            ;;
        'xlibxml2')
            buildopt+=(-Dxml-backend=libxml2)
            ;;
    esac
    if [ $cross -eq 1 -a -n "$arch" ]; then
        buildopt+=(--cross-file)
        buildopt+=(.gitlab-ci/$arch.txt)
        if [ ! -f .gitlab-ci/$FC_DISTRO_NAME-cross.sh ]; then
            echo "No $FC_DISTRO_NAME-cross.sh available"
            exit 1
        fi
        . .gitlab-ci/$FC_DISTRO_NAME-cross.sh
    fi
    buildopt+=(--default-library=$type)
    meson setup --prefix="$PREFIX" -Dnls=enabled -Dcache-build=disabled -Diconv=enabled ${buildopt[*]} "$BUILDDIR" 2>&1 | tee /tmp/fc-build.log || r=$?
    meson compile -v -C "$BUILDDIR" 2>&1 | tee -a /tmp/fc-build.log || r=$?
    if [ $disable_check -eq 0 ]; then
        meson test -v -C "$BUILDDIR" 2>&1 | tee -a /tmp/fc-build.log || r=$?
    fi
    if [ $enable_install -eq 1 ]; then
        meson install -C "$BUILDDIR" 2>&1 | tee -a /tmp/fc-build.log || r=$?
    fi
    if [ $distcheck -eq 1 ]; then
        meson dist -C "$BUILDDIR" 2>&1 | tee -a /tmp/fc-build.log || r=$?
    fi
fi
mv /tmp/fc-build.log . || :
exit $r
