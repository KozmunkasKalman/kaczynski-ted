outdir ?= .

srpm:
	dnf install -y rpmdevtools tar
	mkdir -p rpmbuild/{SOURCES,SPECS}
	tar --transform 's,^,kaczynski-ted/,' --exclude-vcs --exclude rpmbuild -czf rpmbuild/SOURCES/kaczynski-ted.tar.gz .
	cp kaczynski.spec rpmbuild/SPECS/
	rpmbuild -bs --define "_topdir $$(pwd)/rpmbuild" --define "_srcrpmdir $(outdir)" rpmbuild/SPECS/kaczynski.spec