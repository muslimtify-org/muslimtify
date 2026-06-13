# Packaging Guide

This directory contains packaging configurations for distributing muslimtify
across Linux distributions and Windows. Each subdirectory targets a different
package manager and build service:

| Directory | Target platform     | Package format | Build service       | User installs with |
|-----------|---------------------|----------------|---------------------|--------------------|
| `aur/`    | Arch Linux, Manjaro | PKGBUILD       | AUR (user-built)    | `yay -S muslimtify` |
| `debian/` | Ubuntu, Debian      | `.deb`         | Launchpad PPA       | `apt install muslimtify` |
| `fedora/` | Fedora, RHEL, CentOS| `.rpm`         | Fedora COPR         | `dnf install muslimtify` |
| `winget/` | Windows 10+         | Inno Setup     | GitHub Releases     | `winget install rizukirr.muslimtify` |

```
.packages/
├── aur/                    # Arch Linux (AUR)
│   ├── PKGBUILD            # Package recipe — defines version, deps, build steps
│   ├── .SRCINFO            # Metadata parsed by the AUR web interface (auto-generated)
│   └── muslimtify.install  # Post-install/upgrade/remove hook messages
│
├── debian/                 # Debian/Ubuntu (Launchpad PPA)
│   ├── debian/             # Standard Debian packaging directory
│   │   ├── control         # Package metadata — name, deps, description
│   │   ├── rules           # Build recipe (dh + cmake override)
│   │   ├── changelog       # Version history — drives the package version
│   │   ├── copyright       # License info in Debian machine-readable format
│   │   ├── postinst        # Post-install script (prints daemon setup message)
│   │   └── source/format   # Source package format (3.0 quilt)
│   ├── build.sh            # Build .deb binary locally in a debootstrap chroot
│   └── upload-ppa.sh       # Build source package, GPG-sign, upload to Launchpad
│
├── fedora/                 # Fedora (COPR)
│   ├── muslimtify.spec     # RPM spec file — defines metadata, deps, build, file list
│   └── upload-copr.sh      # Build SRPM and upload to Fedora COPR
│
├── winget/                  # Windows (winget / Inno Setup)
│   └── muslimtify.iss      # Inno Setup script — builds the Windows installer
│
└── README.md               # This file
```

---

## Setting Up Keys from Scratch

If you have never created GPG or SSH keys before, follow these steps first. Everything
else in this guide depends on having these keys in place.

### GPG key (required for Launchpad PPA signing)

1. **Generate a new GPG key:**

   ```bash
   gpg --full-generate-key
   ```

   When prompted:
   - Key type: choose **(9) ECC (sign and encrypt)** (or EdDSA/Ed25519 if listed)
   - Elliptic curve: **Curve 25519**
   - Expiration: **0** (does not expire), or pick a duration you prefer
   - Real name: your full name
   - Email: the email you use for Launchpad
   - Passphrase: set a strong passphrase — you will need it every time you sign packages

2. **Verify the key was created:**

   ```bash
   gpg --fingerprint
   ```

   You should see output showing your key's fingerprint (40 hex characters).
   Note the full fingerprint — you will need it.

3. **Upload the key to Ubuntu keyserver:**

   Launchpad can only find your key if it is on the Ubuntu keyserver.

   ```bash
   gpg --keyserver keyserver.ubuntu.com --send-keys <YOUR_FINGERPRINT>
   ```

   Replace `<YOUR_FINGERPRINT>` with the 40-character fingerprint without spaces, e.g.:

   ```bash
   gpg --keyserver keyserver.ubuntu.com --send-keys <YOUR_FINGERPRINT>
   ```

4. **Register the key on Launchpad:**

   - Go to https://launchpad.net/~/+editpgpkeys
   - Paste the fingerprint **without spaces** and submit
   - Launchpad sends an encrypted confirmation email to your key's email address

5. **Decrypt the confirmation email:**

   The email body is a PGP-encrypted message. Copy the entire block (including the
   `-----BEGIN PGP MESSAGE-----` and `-----END PGP MESSAGE-----` lines), then:

   ```bash
   gpg --decrypt
   ```

   Paste the PGP message and press `Ctrl+D`. It will output a confirmation URL.
   Open that URL in your browser to complete the registration.

6. **Verify on Launchpad:**

   Visit your Launchpad profile — your GPG key should now appear under "OpenPGP keys".

### SSH key (required for AUR)

1. **Generate a new SSH key:**

   ```bash
   ssh-keygen -t ed25519 -C "your_email@example.com"
   ```

   When prompted:
   - File: press Enter to accept the default (`~/.ssh/id_ed25519`)
   - Passphrase: set one for security (or leave empty for convenience)

2. **Start the SSH agent and add your key:**

   ```bash
   eval "$(ssh-agent -s)"
   ssh-add ~/.ssh/id_ed25519
   ```

3. **Copy the public key:**

   ```bash
   cat ~/.ssh/id_ed25519.pub
   ```

   Copy the entire output line.

4. **Register the key on AUR:**

   - Create an account at https://aur.archlinux.org/register/ (if you don't have one)
   - Go to https://aur.archlinux.org/account/YourUsername/edit/
   - Paste your public key into the **SSH Public Key** field and save

5. **Test the connection:**

   ```bash
   ssh -T aur@aur.archlinux.org
   ```

   If it works you will see something like `Welcome to AUR, YourUsername!` (the
   connection then closes — this is normal, AUR does not provide shell access).

6. **(Optional) Register the same key on GitHub:**

   - Go to https://github.com/settings/ssh/new
   - Paste the same public key and save
   - Test: `ssh -T git@github.com`

---

## Prerequisites

### Accounts and keys

| Service      | What you need | Link |
|--------------|---------------|------|
| AUR          | SSH key registered on AUR (see above) | https://aur.archlinux.org/ |
| Launchpad    | GPG key registered (see above) | https://launchpad.net/ |
| Fedora COPR  | Fedora Account (FAS) + COPR API token | https://accounts.fedoraproject.org/ |
| winget       | GitHub account + `wingetcreate` CLI | https://github.com/microsoft/winget-pkgs |

### Repository URLs

- **GitHub:** `https://github.com/<YOUR_USERNAME>/muslimtify`
- **AUR:** `ssh://aur@aur.archlinux.org/muslimtify.git`
- **Launchpad PPA:** `ppa:<YOUR_USERNAME>/muslimtify`
- **COPR:** `<YOUR_USERNAME>/muslimtify`

### Packages needed on the build machine (Arch)

```bash
# AUR tools
pacman -S pacman-contrib base-devel     # provides updpkgsums, makepkg

# PPA tools (building .deb on Arch via debootstrap chroot)
pacman -S debootstrap ubuntu-keyring
yay -S dput python-charset-normalizer

# COPR tools (building .rpm)
yay -S copr-cli rpm-tools
```

---

## Releasing a New Version

### Files to update

When you bump the version, these files **must** be updated:

| # | File | What to change |
|---|------|----------------|
| 1 | `src/cli.c` | Version string in the source code |
| 2 | `CMakeLists.txt` | `project(muslimtify VERSION X.Y.Z ...)` |
| 3 | `.packages/aur/PKGBUILD` | `pkgver=X.Y.Z`, reset `pkgrel=1` |
| 4 | `.packages/aur/.SRCINFO` | Regenerate with `makepkg --printsrcinfo > .SRCINFO` |
| 5 | `.packages/debian/debian/changelog` | New entry at the **top** of the file |
| 6 | `.packages/fedora/muslimtify.spec` | `Version: X.Y.Z`, reset `Release: 1%{?dist}`, new `%changelog` entry |
| 7 | `.packages/winget/muslimtify.iss` | `#define MyAppVersion "X.Y.Z"` |

### Version flow

```
1. Update source code (src/cli.c, CMakeLists.txt)
2. Update all packaging files (steps 3-6 above)
3. Commit everything
4. Tag and push:   git tag vX.Y.Z && git push --tags
5. Create GitHub release from the tag
6. Upload to AUR, PPA, COPR, and winget (see sections below)
```

> **Important:** The AUR and COPR source tarballs are downloaded from GitHub releases,
> so the GitHub release **must** exist before you can update checksums or build SRPMs.

---

## AUR (Arch Linux)

### How it works

The AUR (Arch User Repository) hosts `PKGBUILD` files that users build locally with
a helper like `yay` or `paru`. There is no binary — the user's machine downloads the
source tarball from GitHub and compiles it.

**Key files:**

- `PKGBUILD` — The build recipe. Defines `pkgver`, `depends`, `makedepends`, `source`
  URL, `sha256sums`, and the `build()` / `package()` functions.
- `.SRCINFO` — Machine-readable metadata extracted from `PKGBUILD`. The AUR web
  interface parses this, not the `PKGBUILD` itself. Must be regenerated every time
  `PKGBUILD` changes.
- `muslimtify.install` — Optional install hooks. Contains `post_install()`,
  `post_upgrade()`, and `post_remove()` functions that print daemon setup instructions.

### First-time setup

```bash
# Clone the AUR repo (this is a SEPARATE git repo from the main project)
git clone ssh://aur@aur.archlinux.org/muslimtify.git ~/aur-muslimtify
```

The `.packages/aur/` directory in this repo is a convenience copy. When pushing to
AUR, you copy the files into your `~/aur-muslimtify` clone and push from there.

### Updating the AUR package

1. **Update version in PKGBUILD:**

   ```bash
   # Edit .packages/aur/PKGBUILD
   # Change: pkgver=X.Y.Z
   # Reset:  pkgrel=1  (always reset to 1 for a new upstream version)
   ```

2. **Update checksums** (requires the GitHub release tag to exist):

   ```bash
   cd .packages/aur
   updpkgsums  # downloads the tarball and updates sha256sums= in PKGBUILD
   ```

   `updpkgsums` is from the `pacman-contrib` package. It automatically downloads each
   URL in `source=()` and replaces the checksums.

3. **Regenerate .SRCINFO:**

   ```bash
   makepkg --printsrcinfo > .SRCINFO
   ```

4. **Test the build locally:**

   ```bash
   makepkg -si    # -s = install missing deps, -i = install the built package
   makepkg -si -f # -f = force rebuild if already built
   ```

5. **Push to AUR:**

   ```bash
   cd ~/aur-muslimtify
   cp /path/to/project/.packages/aur/{PKGBUILD,.SRCINFO,muslimtify.install} .
   git add PKGBUILD .SRCINFO muslimtify.install
   git commit -m "Update to vX.Y.Z"
   git push
   ```

### AUR update checklist

```
[ ] Update pkgver in PKGBUILD
[ ] Reset pkgrel to 1
[ ] Ensure GitHub release tag exists (git tag vX.Y.Z && git push --tags)
[ ] Run updpkgsums (updates sha256sums)
[ ] Regenerate .SRCINFO (makepkg --printsrcinfo > .SRCINFO)
[ ] Test locally (makepkg -si)
[ ] Copy files to ~/aur-muslimtify, commit, and push
```

### Troubleshooting

- **`updpkgsums` fails with 404:** The GitHub release tag doesn't exist yet. Create it first.
- **`makepkg` fails on checksum mismatch:** Run `updpkgsums` again — someone may have
  re-tagged the release.
- **AUR push rejected:** Make sure your SSH key is registered at https://aur.archlinux.org/
  under your account settings.

---

## Debian/Ubuntu (Launchpad PPA)

### How it works

Launchpad PPAs host source packages that Launchpad builds into `.deb` binaries for
each target architecture. The workflow is:

1. `upload-ppa.sh` creates a debootstrap chroot (since we develop on Arch, not Ubuntu)
2. Inside the chroot, it builds an **unsigned source package** (`.dsc` + `.orig.tar.gz`)
3. Back on the host, it **GPG-signs** the `.dsc` and `.changes` files
4. You upload the signed source package to Launchpad with `dput`
5. Launchpad receives the source, builds `.deb` binaries, and publishes them to the PPA

Users then add the PPA and install with `apt`.

**Key files:**

- `debian/control` — Package metadata: name, build dependencies, runtime dependencies,
  description. This is where `libnotify-dev` and `libcurl4-openssl-dev` are declared as
  build deps.
- `debian/rules` — The build recipe. Minimal: uses `dh` (debhelper) with a single
  override to pass `-DCMAKE_BUILD_TYPE=Release` to CMake.
- `debian/changelog` — Version history in a strict Debian format. The **first entry**
  determines the package version. Must use RFC 2822 dates.
- `debian/copyright` — License info in Debian machine-readable format (MIT).
- `debian/postinst` — Post-install script that prints a message telling the user to
  run `muslimtify daemon install`.
- `debian/source/format` — Declares source format `3.0 (quilt)`.
- `build.sh` — Builds a `.deb` binary locally inside a debootstrap chroot (for testing).
- `upload-ppa.sh` — Builds a source package in a chroot, GPG-signs it on the host,
  and prompts you to upload via `dput`.

### First-time setup

1. **Register your GPG key on Launchpad:**

   Your GPG key must be registered at
   `https://launchpad.net/~<YOUR_USERNAME>/+editpgpkeys`

2. **Install tools on Arch:**

   ```bash
   sudo pacman -S debootstrap ubuntu-keyring
   yay -S dput python-charset-normalizer
   ```

3. **The PPA already exists at:** `https://launchpad.net/~<YOUR_USERNAME>/+archive/ubuntu/muslimtify`

### Updating the PPA

1. **Add a new changelog entry** at the **top** of `.packages/debian/debian/changelog`:

   ```
   muslimtify (X.Y.Z-1) unstable; urgency=medium

     * Description of changes.

    -- Your Name <your_email@example.com>  Thu, 27 Feb 2026 00:00:00 +0700
   ```

   > The date format **must** be RFC 2822. Generate it with `date -R`.
   > The two spaces before `--` are required.

2. **Clean old build artifacts:**

   ```bash
   rm -f .packages/debian/muslimtify_*
   ```

3. **Build, sign, and upload:**

   ```bash
   sudo .packages/debian/upload-ppa.sh
   ```

   This script will:
   - Create/reuse a debootstrap chroot at `~/.cache/muslimtify-deb-chroot`
   - Mount the project directory into the chroot
   - Install build dependencies inside the chroot
   - Build an unsigned source package (`.dsc`, `.orig.tar.gz`, `.debian.tar.xz`)
   - Sign the `.dsc` and `.changes` files with your GPG key on the host
   - Update checksums in `.changes` to match the signed `.dsc`
   - Prompt you to upload via `dput`

4. **Check build status:**

   `https://launchpad.net/~<YOUR_USERNAME>/+archive/ubuntu/muslimtify/+packages`

   You'll also receive an email when the build completes or fails.

### Targeting multiple Ubuntu series

By default, `upload-ppa.sh` targets Ubuntu 24.04 (noble). To upload for other series:

```bash
# Clean between series
rm -f .packages/debian/muslimtify_*

# Upload for each target
sudo .packages/debian/upload-ppa.sh noble    # Ubuntu 24.04
sudo .packages/debian/upload-ppa.sh jammy    # Ubuntu 22.04
```

Each series needs a separate upload with a **distinct version**. Edit the changelog to use a
version suffix like `0.1.3-1~jammy1` for jammy and `0.1.3-1~noble1` for noble.

### Building a .deb locally (for testing)

```bash
sudo .packages/debian/build.sh
# Output: .packages/debian/muslimtify_X.Y.Z-1_amd64.deb

# Test install
sudo dpkg -i .packages/debian/muslimtify_*_amd64.deb
sudo apt install -f  # fix dependencies if needed
```

`build.sh` works similarly to `upload-ppa.sh` but produces a binary `.deb` instead of
a source package. It uses `dpkg-buildpackage -us -uc -b` (unsigned, binary-only).

### Re-uploading after rejection

If Launchpad rejects your upload or you need to re-upload:

```bash
rm -f .packages/debian/muslimtify_*
dput --force ppa:<YOUR_USERNAME>/muslimtify .packages/debian/muslimtify_*_source.changes
```

> **Note:** Launchpad will **not** accept the same version twice. If you need to
> re-upload, bump the version (e.g., `0.1.3-1` → `0.1.3-2`) in `debian/changelog`.

### Deleting the chroot

The debootstrap chroot is cached at `~/.cache/muslimtify-deb-chroot` (~500MB).
To free disk space:

```bash
sudo rm -rf ~/.cache/muslimtify-deb-chroot
```

It will be recreated automatically on the next build.

### Troubleshooting

- **GPG signing fails:** Make sure your signing key is in your GPG keyring
  and not expired. Test with `gpg --list-keys <YOUR_KEY_ID>`.
- **Launchpad rejects "already uploaded":** Bump the `-N` suffix in the changelog
  (e.g., `-1` → `-2`).
- **Build fails inside chroot:** Delete the chroot (`sudo rm -rf ~/.cache/muslimtify-deb-chroot`)
  and rebuild — it may be corrupted or stale.
- **`dput` fails with "already uploaded":** Remove `~/.dput.cf` upload records or use
  `dput --force`.

---

## Fedora (COPR)

### How it works

Fedora COPR (Cool Other Package Repositories) is the Fedora equivalent of Ubuntu PPAs
or the AUR. The workflow is:

1. `upload-copr.sh` downloads the source tarball from GitHub
2. It builds an **SRPM** (source RPM) locally using `rpmbuild`
3. You upload the SRPM to COPR via `copr-cli`
4. COPR builds binary RPMs for each enabled chroot (Fedora version + architecture)
5. Users enable the COPR repo and install with `dnf`

**Key files:**

- `muslimtify.spec` — The RPM spec file. Contains all metadata, build instructions,
  and the file manifest in one file. Sections:
  - Header: `Name`, `Version`, `Release`, `Summary`, `License`, `URL`, `Source0`
  - `BuildRequires` / `Requires`: build-time and runtime dependencies
  - `%description`: long description
  - `%prep`: source extraction (`%autosetup` unpacks and patches)
  - `%build`: compile steps (`%cmake` + `%cmake_build` macros)
  - `%install`: install into the buildroot (`%cmake_install`)
  - `%post`: post-install script (prints daemon setup message)
  - `%files`: explicit list of every file the package owns
  - `%changelog`: version history (newest entry at the **top**)
- `upload-copr.sh` — Downloads the source tarball, builds an SRPM in a temp directory
  with `rpmbuild -bs`, copies the result to `.packages/fedora/`, and optionally uploads
  to COPR.

### First-time setup

1. **Create a Fedora Account (FAS):**

   Register at https://accounts.fedoraproject.org/

2. **Get your COPR API token:**

   Visit https://copr.fedorainfracloud.org/api/ — log in with your FAS credentials
   and it will show you a config block. Save it to `~/.config/copr`:

   ```ini
   [copr-cli]
   login = <your-login>
   username = <your-username>
   token = <your-token>
   copr_url = https://copr.fedorainfracloud.org
   ```

3. **Install tools:**

   ```bash
   # On Fedora
   sudo dnf install copr-cli rpm-build

   # On Arch
   yay -S copr-cli rpm-tools
   ```

4. **Create the COPR project** (one-time):

   ```bash
   copr-cli create muslimtify \
       --chroot fedora-rawhide-x86_64 \
       --chroot fedora-41-x86_64 \
       --chroot fedora-40-x86_64 \
       --description "Prayer time notifier for the desktop" \
       --instructions 'sudo dnf copr enable <YOUR_USERNAME>/muslimtify && sudo dnf install muslimtify'
   ```

   You can also create it via the web UI at `https://copr.fedorainfracloud.org/coprs/<YOUR_USERNAME>/`

   To add more chroots later:

   ```bash
   copr-cli modify muslimtify --chroot fedora-42-x86_64
   ```

### Updating the COPR package

1. **Update the spec file:**

   ```bash
   # Edit .packages/fedora/muslimtify.spec
   # Change: Version: X.Y.Z
   # Reset:  Release: 1%{?dist}
   # Add new %changelog entry at the TOP of the %changelog section:
   ```

   ```
   %changelog
   * Mon Mar 03 2026 Your Name <your_email@example.com> - X.Y.Z-1
   - Release vX.Y.Z
   ```

   > The date format is: `* Day-of-week Month DD YYYY Name <email> - Version-Release`
   > Generate the date portion with `date +"%a %b %d %Y"`.

2. **Build SRPM and upload:**

   ```bash
   .packages/fedora/upload-copr.sh
   ```

   This script will:
   - Read the version from `muslimtify.spec`
   - Download the source tarball from the GitHub release archive URL
     (or use a cached copy if already downloaded)
   - Create a temporary `rpmbuild` tree in `/tmp`
   - Run `rpmbuild -bs` to produce an SRPM
   - Copy the SRPM to `.packages/fedora/`
   - If `copr-cli` is installed, prompt you to upload
   - If `copr-cli` is not installed, print manual instructions

3. **Check build status:**

   `https://copr.fedorainfracloud.org/coprs/<YOUR_USERNAME>/`muslimtify/builds/

### Building an RPM locally (for testing)

```bash
# On Fedora — install build dependencies
sudo dnf install cmake gcc pkgconfig libnotify-devel libcurl-devel rpm-build

# Build the SRPM first (answer N to the upload prompt)
.packages/fedora/upload-copr.sh

# Option 1: Build RPM from SRPM using mock (recommended — builds in a clean chroot)
sudo dnf install mock
sudo usermod -aG mock $USER  # then re-login
mock -r fedora-41-x86_64 .packages/fedora/muslimtify-*.src.rpm

# Option 2: Build directly with rpmbuild (uses your system, not a chroot)
rpmbuild --rebuild .packages/fedora/muslimtify-*.src.rpm
# Output goes to ~/rpmbuild/RPMS/x86_64/
```

### User installation

Once published, Fedora users install with:

```bash
sudo dnf copr enable <YOUR_USERNAME>/muslimtify
sudo dnf install muslimtify
```

To uninstall:

```bash
muslimtify daemon uninstall            # remove the systemd timer first
sudo dnf remove muslimtify
sudo dnf copr disable <YOUR_USERNAME>/muslimtify
```

### COPR update checklist

```
[ ] Update Version in muslimtify.spec
[ ] Reset Release to 1%{?dist}
[ ] Add new %changelog entry
[ ] Ensure GitHub release tag exists
[ ] Run upload-copr.sh
[ ] Verify build at `https://copr.fedorainfracloud.org/coprs/<YOUR_USERNAME>/`muslimtify/builds/
```

### Troubleshooting

- **`rpmbuild` fails with "File not found":** The GitHub release tag doesn't exist yet.
  Create and push the tag first.
- **`copr-cli` auth error:** Regenerate your API token at https://copr.fedorainfracloud.org/api/
  and save it to `~/.config/copr`.
- **COPR build fails on missing deps:** Check that `BuildRequires` in the spec matches
  the Fedora package names. Use `dnf provides '*/libnotify.pc'` to find the right package.
- **COPR rejects duplicate build:** Unlike Launchpad, COPR allows rebuilding the same
  version. But if you need a new build with changes, bump `Release` (e.g., `1%{?dist}` →
  `2%{?dist}`).

---

## Windows (winget / Inno Setup)

### How it works

The Windows installer is built with Inno Setup. The workflow is:

1. Build the project with CMake and install to a staging directory
2. Compile the Inno Setup script into a `muslimtify-X.Y.Z-setup.exe` installer
3. Upload the installer to a GitHub release
4. Submit a manifest to the `microsoft/winget-pkgs` repository via `wingetcreate`
5. Microsoft reviews and merges the PR — the package becomes available via `winget install`

The installer handles everything automatically:
- Installs to `%LOCALAPPDATA%\Programs\Muslimtify` (per-user, no admin required)
- Creates a Start Menu shortcut with AUMID (required for toast notifications)
- Adds `bin\` to the user PATH
- Registers the scheduled task via `muslimtify daemon install`
- On uninstall: removes the scheduled task, PATH entry, shortcut, and cache

**Key files:**

- `muslimtify.iss` — Inno Setup script. Defines the installer sections:
  - `[Setup]`: metadata, install location (`{localappdata}\Programs\Muslimtify`),
    `PrivilegesRequired=lowest`
  - `[Files]`: binaries, DLLs, icon from the staging directory
  - `[Icons]`: Start Menu shortcut with `AppUserModelID` for toast notifications
  - `[Registry]`: adds `{app}\bin` to user PATH
  - `[Run]`: runs `muslimtify daemon install` post-install
  - `[UninstallRun]`: runs `muslimtify daemon uninstall` before uninstall
  - `[Code]`: Pascal script for PATH add/remove logic

### Prerequisites

1. **Inno Setup 6+** installed on Windows:

   Download from https://jrsoftware.org/isdl.php

2. **CMake + MSVC** (for building the project):

   The project uses MSVC on Windows. Visual Studio Build Tools or full Visual Studio
   with the C/C++ workload is required.

3. **`wingetcreate`** (for submitting to winget):

   ```powershell
   winget install wingetcreate
   ```

4. **GitHub Personal Access Token** (for `wingetcreate` to create PRs):

   Create a token at https://github.com/settings/tokens with `public_repo` scope.

### Building the installer

1. **Build the project and install to staging:**

   ```powershell
   cmake -B build
   cmake --build build --config Release
   cmake --install build --config Release --prefix installer/staging
   ```

   This produces:
   ```
   installer/staging/
     bin/
       muslimtify.exe
       muslimtify-service.exe
       *.dll              (runtime dependencies)
     share/
       icons/
         muslimtify.ico
   ```

2. **Compile the Inno Setup installer:**

   ```powershell
   & "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" .packages\winget\muslimtify.iss
   ```

   Output: `dist\muslimtify-X.Y.Z-setup.exe`

3. **Test the installer:**

   ```powershell
   # Interactive install
   .\dist\muslimtify-X.Y.Z-setup.exe

   # Silent install (how winget runs it)
   .\dist\muslimtify-X.Y.Z-setup.exe /VERYSILENT /SUPPRESSMSGBOXES
   ```

   Verify:
   - `muslimtify` is available in a new terminal
   - Start Menu shortcut exists with the icon
   - `muslimtify daemon status` shows the scheduled task
   - Uninstall from Settings > Apps cleans everything up

### First-time winget submission

This is a one-time process to register the package in the winget repository.

1. **Create a GitHub release** with the installer attached:

   ```bash
   git tag vX.Y.Z && git push --tags
   ```

   Then create a release at `https://github.com/<YOUR_USERNAME>/muslimtify/releases/new`,
   upload `dist/muslimtify-X.Y.Z-setup.exe` as a release asset.

2. **Generate and submit the winget manifest:**

   ```powershell
   wingetcreate new <INSTALLER_URL>
   ```

   Replace `<INSTALLER_URL>` with the direct download URL of the installer from the
   GitHub release, e.g.:

   ```
   https://github.com/<YOUR_USERNAME>/muslimtify/releases/download/vX.Y.Z/muslimtify-X.Y.Z-setup.exe
   ```

   `wingetcreate` will prompt you for:

   | Field | Value |
   |-------|-------|
   | Package identifier | `rizukirr.muslimtify` |
   | Package version | `X.Y.Z` |
   | Package locale | `en-US` |
   | Publisher | `rizukirr` |
   | Package name | `Muslimtify` |
   | License | `MIT` |
   | Short description | `Prayer time notification daemon for Windows` |
   | Installer type | Should auto-detect as `inno` |

   It will then ask to submit a PR — provide your GitHub token when prompted.

3. **Wait for review:**

   `wingetcreate` creates a PR to `microsoft/winget-pkgs`. Microsoft's validation bot
   runs automated checks, then a human reviewer approves the PR. This typically takes
   a few days.

   Track your PR at: https://github.com/microsoft/winget-pkgs/pulls

### Updating the winget package

For subsequent releases, use `wingetcreate update`:

```powershell
wingetcreate update rizukirr.muslimtify `
  --version X.Y.Z `
  --urls <NEW_INSTALLER_URL>|x64`
  --submit `
```

This automatically creates a PR with the updated manifest.

### Alternative: manual manifest submission

If you prefer not to use `wingetcreate`, you can manually create a manifest and
submit a PR to `microsoft/winget-pkgs`. The manifest consists of three YAML files
in `manifests/r/rizukirr/muslimtify/X.Y.Z/`:

**`rizukirr.muslimtify.installer.yaml`:**
```yaml
PackageIdentifier: rizukirr.muslimtify
PackageVersion: X.Y.Z
InstallerType: inno
Installers:
  - Architecture: x64
    InstallerUrl: https://github.com/<YOUR_USERNAME>/muslimtify/releases/download/vX.Y.Z/muslimtify-X.Y.Z-setup.exe
    InstallerSha256: <SHA256_OF_INSTALLER>
    InstallerSwitches:
      Silent: /VERYSILENT /SUPPRESSMSGBOXES
      SilentWithProgress: /SILENT
ManifestType: installer
ManifestVersion: 1.9.0
```

**`rizukirr.muslimtify.locale.en-US.yaml`:**
```yaml
PackageIdentifier: rizukirr.muslimtify
PackageVersion: X.Y.Z
PackageLocale: en-US
Publisher: rizukirr
PackageName: Muslimtify
License: MIT
ShortDescription: Prayer time notification daemon for Windows
PackageUrl: https://github.com/<YOUR_USERNAME>/muslimtify
ManifestType: defaultLocale
ManifestVersion: 1.9.0
```

**`rizukirr.muslimtify.yaml`:**
```yaml
PackageIdentifier: rizukirr.muslimtify
PackageVersion: X.Y.Z
DefaultLocale: en-US
ManifestType: version
ManifestVersion: 1.9.0
```

Generate the SHA256:
```powershell
(Get-FileHash .\dist\muslimtify-X.Y.Z-setup.exe -Algorithm SHA256).Hash
```

Fork `microsoft/winget-pkgs`, create the files under `manifests/r/rizukirr/muslimtify/X.Y.Z/`,
and submit a PR.

### winget update checklist

```
[ ] Update #define MyAppVersion in .packages/winget/muslimtify.iss
[ ] Build project: cmake --build build --config Release
[ ] Install to staging: cmake --install build --config Release --prefix installer/staging
[ ] Compile installer: ISCC.exe .packages/winget/muslimtify.iss
[ ] Test installer (interactive + silent)
[ ] Create GitHub release, upload installer
[ ] Submit manifest: wingetcreate update rizukirr.muslimtify --version X.Y.Z --urls <URL> --submit --token <PAT>
[ ] Track PR: https://github.com/microsoft/winget-pkgs/pulls
```

### Troubleshooting

- **`ISCC.exe` not found:** Install Inno Setup 6 from https://jrsoftware.org/isdl.php.
  Default install path is `C:\Program Files (x86)\Inno Setup 6\`.
- **Staging directory missing files:** Re-run `cmake --install build --config Release --prefix installer/staging`.
  Make sure the build succeeded first.
- **`wingetcreate` validation fails:** Check that the installer URL is a direct download
  link (not a redirect to a HTML page). GitHub release asset URLs work.
- **winget PR rejected:** Read the review comments. Common issues: missing required fields,
  SHA256 mismatch, or installer URL not publicly accessible.
- **Toast notifications don't work after install:** The Start Menu shortcut must have the
  correct AUMID (`Muslimtify`). Verify the shortcut exists in
  `%APPDATA%\Microsoft\Windows\Start Menu\Programs\Muslimtify.lnk`.

---

## Full Release Checklist

Complete walkthrough for publishing a new version to all four platforms.

### 1. Prepare the release

```
[ ] Update version string in src/cli.c
[ ] Update version in CMakeLists.txt: project(muslimtify VERSION X.Y.Z ...)
[ ] Update .packages/aur/PKGBUILD (pkgver=X.Y.Z, pkgrel=1)
[ ] Update .packages/debian/debian/changelog (new entry at top, date -R for timestamp)
[ ] Update .packages/fedora/muslimtify.spec (Version, Release, %changelog)
[ ] Update .packages/winget/muslimtify.iss (#define MyAppVersion "X.Y.Z")
[ ] Commit all changes
[ ] Tag and push: git tag vX.Y.Z && git push && git push --tags
[ ] Create GitHub release from the tag at https://github.com/<YOUR_USERNAME>/muslimtify/releases/new
```

### 2. Publish to AUR

```
[ ] cd .packages/aur && updpkgsums
[ ] makepkg --printsrcinfo > .SRCINFO
[ ] Test: makepkg -si
[ ] Copy PKGBUILD, .SRCINFO, muslimtify.install to ~/aur-muslimtify
[ ] cd ~/aur-muslimtify && git add -A && git commit -m "Update to vX.Y.Z" && git push
```

### 3. Publish to Launchpad PPA

```
[ ] rm -f .packages/debian/muslimtify_*
[ ] sudo .packages/debian/upload-ppa.sh
[ ] Verify: https://launchpad.net/~<YOUR_USERNAME>/+archive/ubuntu/muslimtify/+packages
```

### 4. Publish to Fedora COPR

```
[ ] rm -f .packages/fedora/muslimtify-*.src.rpm
[ ] .packages/fedora/upload-copr.sh
[ ] Verify: `https://copr.fedorainfracloud.org/coprs/<YOUR_USERNAME>/`muslimtify/builds/
```

### 5. Publish to winget

```
[ ] Build installer: cmake --build build --config Release
[ ] Install to staging: cmake --install build --config Release --prefix installer/staging
[ ] Compile: ISCC.exe .packages/winget/muslimtify.iss
[ ] Test installer locally (interactive + silent)
[ ] Upload dist/muslimtify-X.Y.Z-setup.exe to the GitHub release
[ ] Submit: wingetcreate update rizukirr.muslimtify --version X.Y.Z --urls <INSTALLER_URL> --submit --token <PAT>
[ ] Track PR: https://github.com/microsoft/winget-pkgs/pulls
```

### 6. Post-release verification

```
[ ] Arch:    yay -S muslimtify (or paru -S muslimtify)
[ ] Ubuntu:  sudo apt update && sudo apt install muslimtify
[ ] Fedora:  sudo dnf copr enable <YOUR_USERNAME>/muslimtify && sudo dnf install muslimtify
[ ] Windows: winget install rizukirr.muslimtify
```
