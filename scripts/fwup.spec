%global __os_install_post %{nil}
%global _build_id_links none

Name:           fwup
Version:        %{fwup_version}
Release:        %{fwup_release}
Summary:        Configurable embedded firmware update creator and runner
License:        Apache-2.0
URL:            https://github.com/fwup-home/fwup

%description
fwup is a configurable embedded Linux firmware update creator and runner.

%prep

%build

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
cp -a %{fwup_staging_dir}/. %{buildroot}/
gzip -9 -n %{buildroot}/usr/share/man/man1/fwup.1
mkdir -p %{buildroot}/usr/share/licenses/fwup
install -m 0644 %{fwup_license} %{buildroot}/usr/share/licenses/fwup/LICENSE

%files
%dir /usr/share/licenses/fwup
%license /usr/share/licenses/fwup/LICENSE
/usr/bin/fwup
/usr/bin/img2fwup
/usr/share/man/man1/fwup.1*
/usr/share/bash-completion/completions/fwup
