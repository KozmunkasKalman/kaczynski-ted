#Jan 1, 2026 Vani1-2 <giovannirafanan609@gmail.com>

CXX = g++
CXXFLAGS = -std=c++23 -lncursesw
TARGET = bin/kcz
SRC = src/main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

#completely optional btw, but I made some macros

VERSION = 0.0.1.alpha1
PWD = $(shell pwd)

pkg: $(TARGET)
	@mkdir -p arch-pkg
	@echo "Generating PKGBUILD..."
	@echo "pkgname=kaczynski" > PKGBUILD
	@echo "pkgver=$(VERSION)" >> PKGBUILD
	@echo "pkgrel=0.0.1-alpha1" >> PKGBUILD
	@echo "pkgdesc=\"Pragmatic modal plaintext editor inspired by Vim, Nano, and such.\"" >> PKGBUILD
	@echo "arch=('x86_64')" >> PKGBUILD
	@echo "license=('Apache2')" >> PKGBUILD
	@echo "package() {" >> PKGBUILD
	@echo "  install -Dm755 $(PWD)/$(TARGET) \$${pkgdir}/usr/bin/kcz" >> PKGBUILD
	@echo "}" >> PKGBUILD
	PKGDEST=arch-pkg PKGEXT='.pkg.tar.xz' makepkg -f

rpm: $(TARGET)
	@echo "Generating RPM spec..."
	@mkdir -p rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
	@echo "Name: kaczynski" > kaczynski.spec
	@echo "Version: $(VERSION)" >> kaczynski.spec
	@echo "Release: 0.0.1-alpha1" >> kaczynski.spec
	@echo "Summary: Pragmatic modal plaintext editor inspired by Vim, Nano, and such." >> kaczynski.spec
	@echo "License: Apache2" >> kaczynski.spec
	@echo "%description" >> kaczynski.spec
	@echo "The Kaczynski text editor." >> kaczynski.spec
	@echo "%install" >> kaczynski.spec
	@echo "install -Dm755 $(PWD)/$(TARGET) %{buildroot}/usr/bin/kcz" >> kaczynski.spec
	@echo "%files" >> kaczynski.spec
	@echo "/usr/bin/kcz" >> kaczynski.spec
	rpmbuild --define "_topdir $(PWD)/rpmbuild" -bb kaczynski.spec
