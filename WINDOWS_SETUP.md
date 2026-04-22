# Ledger: Auto-Update Setup Guide

This guide walks you through installing Ledger as a Windows executable with automatic updates.

---

## 1. Prepare Your GitHub Repository

### If you don't have a repo yet:
```bash
cd /home/machine/Ledger
git init
git add .
git commit -m "Initial commit: Ledger with auto-update"
git branch -M main
git remote add origin https://github.com/YOUR_USERNAME/YOUR_REPO.git
git push -u origin main
```

### Configure the updater for your repo:
Edit [core/updater.cpp](core/updater.cpp) — lines 14–15:
```cpp
static const char *GITHUB_OWNER = "YOUR_GITHUB_USERNAME";
static const char *GITHUB_REPO  = "YOUR_REPO_NAME";
```

Example:
```cpp
static const char *GITHUB_OWNER = "john-smith";
static const char *GITHUB_REPO  = "ledger-trading";
```

---

## 2. GitHub Actions Workflow (Already Created)

The workflow file [.github/workflows/release.yml](.github/workflows/release.yml) is ready. It:
- **Triggers** on any git tag push (e.g., `git tag v1.0.0`)
- **Builds** on Windows with MSVC + Qt6
- **Bundles** all Qt DLLs using `windeployqt`
- **Creates** a GitHub Release with the `.zip` file attached

**Commit the workflow:**
```bash
git add .github/workflows/release.yml
git commit -m "Add GitHub Actions CI/CD"
git push
```

---

## 3. Create Your First Release

From your Linux machine, create a version tag and push it:
```bash
git tag v1.0.0
git push --tags
```

Watch the build on GitHub:
1. Go to your repo: `https://github.com/YOUR_USERNAME/YOUR_REPO`
2. Click **Actions** tab → you'll see `Build & Release (Windows)` running
3. Once done (~5–10 min), check **Releases** tab → you'll see `v1.0.0` with `Ledger-v1.0.0.zip` attached

**Download and extract** this zip on your Windows machine. You now have a portable folder with:
- `Ledger.exe`
- All required Qt DLLs (Qt6Core.dll, Qt6Gui.dll, etc.)
- Plugins folder (platforms, imageformats, etc.)

---

## 4. Install on Windows

Two options:

### Option A: Portable Folder (No Installer)
Simply extract `Ledger-v1.0.0.zip` to your desired location:
- `C:\Users\YourName\AppData\Local\Programs\Ledger\`
- Or anywhere you prefer

Create a shortcut to `Ledger.exe` on your desktop.

### Option B: Installer (Advanced, Optional)
Use **NSIS** or **WiX** to create a proper Windows installer. (Not included in this guide but can be added to the workflow later.)

---

## 5. In-App Update Button

Once installed, the Settings tab has a **"Check for Updates"** button:

1. Click it → queries GitHub API for new releases
2. If a newer version exists, prompts you to download
3. Click **Yes** → downloads the `.zip` silently
4. A PowerShell script auto-extracts and replaces your files
5. App restarts with the new version

**Automatic behavior:** Files are swapped while the app is closed, so no file-in-use errors.

---

## 6. Workflow: Making Updates

Every time you want to release a new version:

```bash
# 1. Make code changes, test locally
# 2. Commit
git add .
git commit -m "Fix: improve trade entry validation"

# 3. Create a version tag (follow semantic versioning: v1.0.0, v1.0.1, v1.1.0, etc.)
git tag v1.0.1

# 4. Push both commits and tag
git push
git push --tags
```

**That's it.** GitHub Actions automatically builds, bundles, and creates a Release. Your users see "Check for Updates" detect the new version.

---

## 7. Troubleshooting

### Build fails on GitHub Actions
- Check the **Actions** tab → view the failed workflow logs
- Common causes:
  - `GITHUB_OWNER` or `GITHUB_REPO` not configured in `updater.cpp`
  - Missing `git push --tags` (the workflow only triggers on tags)
  - Qt6 version mismatch (workflow uses 6.6.3)

### Update button doesn't work
- Check internet connection
- Ensure `GITHUB_OWNER` and `GITHUB_REPO` match your actual GitHub account/repo
- Check Windows firewall isn't blocking the app

### App won't restart after update
- Ensure `Ledger.exe` is in the same folder as the `powershell` script (`C:\Users\YourName\AppData\Local\Temp\ledger_update.ps1`)
- Windows may have file-locking issues; try running the app as Administrator

---

## 8. Key Files Modified

- **core/updater.h** / **core/updater.cpp** — Update checker + auto-installer
- **CMakeLists.txt** — Added `Qt6::Network` dependency, `APP_VERSION` compile flag
- **ui/mainwindow.h** / **ui/mainwindow.cpp** — Settings tab now includes "Check for Updates" button
- **.github/workflows/release.yml** — CI/CD pipeline (auto-generated)

---

## 9. Next Steps (Optional Enhancements)

- Add **versioning badge** to README showing latest release
- Implement **delta updates** (download only changed files instead of full .zip)
- Add **rollback** button to downgrade to a previous version
- Create **NSIS installer** for traditional Windows installation experience
- Add **scheduled update checks** (e.g., check weekly in the background)

---

## 10. Security Notes

- GitHub Releases are public — everyone can see your version history
- If you want private releases, use private GitHub repos (requires PAT token in workflow)
- PowerShell scripts run with your user privileges — ensure the updater code is trusted

