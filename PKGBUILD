pkgname=kcz
pkgver=0.0.1.rc1
pkgrel=1
pkgdesc="Pragmatic modal plaintext editor inspired by Vim, Nano, and such."
arch=('x86_64')
url="https://github.com/KozmunkasKalman/kaczynski-ted.git"
license=('Apache')
depends=('ncurses' 'glibc' 'gcc-libs')
makedepends=('make' 'gcc')
source=("kcz-${pkgver}.tar.gz::file:///${startdir}") 
# If you are just building locally without a tarball, you can often skip 'source' and just copy in package() 
# or use a git source. 
# For a local folder build, this is a common placeholder:
source=()

build() {
    cd "$startdir"
    make
}

package() {
    cd "$startdir"
    install -Dm755 bin/kcz "${pkgdir}/usr/bin/kcz"
    install -Dm644 unabombrc "${pkgdir}/usr/share/kaczynski/unabombrc"
    install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
}