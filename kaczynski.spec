%global debug_package %{nil}

Name:           kcz
Version:        0.1.0.alpha1
Release:        1%{?dist}
Summary:        Pragmatic modal plaintext editor inspired by Vim, Nano, and others

License:        Apache-2.0
URL:            https://github.com/KozmunkasKalman/kaczynski-ted
Source0:        kaczynski-ted.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  ncurses-devel
BuildRequires:  ncurses-static
BuildRequires:  make
Requires:       ncurses

%description
"Kaczynski" Text EDitor is an entirely pragmatic modal text editor inspired by Vim, 
Nano and others. It depends on NCurses for the TUI.

%prep
%setup -q -n kaczynski-ted

%build
make all

%install

install -Dm755 bin/kcz %{buildroot}%{_bindir}/kcz


install -Dm644 README.md %{buildroot}%{_docdir}/%{name}/README.md

%files
%{_bindir}/kcz
%dir %{_docdir}/%{name}
%{_docdir}/%{name}/README.md

%changelog
* Wed Jan 14 2026 Vani1-2 <giovannirafanan609@gmail.com> - 0.1.0.alpha1-1
- Initial package