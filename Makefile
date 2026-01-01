#Jan 1, 2026 Vani1-2 <giovannirafanan609@gmail.com>



CXX = g++
CXXFLAGS = -std=c++23 -lncurses
TARGET = bin/kcz
SRC = src/main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

#completely optional btw, but I made a some macros

VERSION = 0.0.1.alpha1
PWD = $(shell pwd)


pkg: $(TARGET)
	@mkdir -p arch-pkg
	@echo "Generating PKGBUILD..."
	@echo "pkgname=kaczynski-ted" > PKGBUILD
	@echo "pkgver=$(VERSION)" >> PKGBUILD
	@echo "pkgrel=1" >> PKGBUILD
	@echo "pkgdesc=\"A pragmatic modal text editor\"" >> PKGBUILD
	@echo "arch=('x86_64')" >> PKGBUILD
	@echo "license=('MIT')" >> PKGBUILD
	@echo "package() {" >> PKGBUILD
	@echo "  install -Dm755 $(PWD)/$(TARGET) \$${pkgdir}/usr/bin/kcz" >> PKGBUILD
	@echo "}" >> PKGBUILD
	PKGDEST=arch-pkg PKGEXT='.pkg.tar.xz' makepkg -f


rpm: $(TARGET)
	@echo "Generating RPM spec..."
	@mkdir -p rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
	@echo "Name: kaczynski-ted" > kaczynski.spec
	@echo "Version: $(VERSION)" >> kaczynski.spec
	@echo "Release: 1" >> kaczynski.spec
	@echo "Summary: modal editor based on vim and the like" >> kaczynski.spec
	@echo "License: MIT" >> kaczynski.spec
	@echo "%description" >> kaczynski.spec
	@echo "The Kaczynski text editor." >> kaczynski.spec
	@echo "%install" >> kaczynski.spec
	@echo "install -Dm755 $(PWD)/$(TARGET) %{buildroot}/usr/bin/kcz" >> kaczynski.spec
	@echo "%files" >> kaczynski.spec
	@echo "/usr/bin/kcz" >> kaczynski.spec
	rpmbuild --define "_topdir $(PWD)/rpmbuild" -bb kaczynski.spec
