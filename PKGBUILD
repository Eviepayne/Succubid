# Maintainer: UnknownPleasuresDev <unknownpleasuresdev@proton.me>
pkgname=succubid
pkgver=1.0.0
pkgrel=1
pkgdesc="synchronize MPV with The Handy"
arch=('x86_64' 'aarch64')
url="https://github.com/UnknownPleasuresDev/Succubid"
license=('AGPL-3.0-or-later')
depends=('curl' 'mpv')
makedepends=('xxd' 'nlohmann-json' 'cpp-httplib')
options=('!debug')

prepare() {
  cd "$startdir"
  make clean
}

build() {
  cd "$startdir"
  make BUILD=release
}

package() {
  cd "$startdir"

  install -Dm755 succubid "$pkgdir/usr/bin/succubid"
  install -Dm644 succubid.1 "$pkgdir/usr/share/man/man1/succubid.1"
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
