# SiteGuard

GTK3 app for daily website time budgets on Linux. Uses [ActivityWatch](https://activitywatch.net/)
for usage tracking and `/etc/hosts` for OS-level blocking.

## Architecture

```
siteguard (GTK3 UI + CLI)
    │
    ├── reads ~/.config/siteguard/config.conf
    ├── queries ActivityWatch REST API (localhost:5600) every 60s
    ├── forks siteguard-helper to block/unblock via /etc/hosts
    │
siteguard-helper (setuid root)
    │
    └── only binary that writes /etc/hosts
        validates domain, atomic rename on unblock

systemd (user timers)
    ├── siteguard-check.timer  → every 5 min → siteguard --check
    └── siteguard-reset.timer  → midnight    → siteguard --reset
```

## Dependencies

| Package      | Arch package   | Purpose                          |
|--------------|----------------|----------------------------------|
| GTK 3        | `gtk3`         | UI toolkit                       |
| libcurl      | `curl`         | HTTP calls to ActivityWatch API  |
| json-c       | `json-c`       | JSON parsing                     |
| ActivityWatch | AUR: `activitywatch-bin` | Usage tracking         |
| AW web watcher | AUR: `aw-watcher-web-firefox` or `-chromium` | Browser events |

```bash
# System packages
sudo pacman -S gtk3 curl json-c

# AUR (use yay, paru, or manual)
yay -S activitywatch-bin
yay -S aw-watcher-web-firefox    # Firefox
yay -S aw-watcher-web-chromium   # Chromium/Chrome (if needed)
```

## Build & Install

```bash
# Check deps first
make deps

# Build
make

# Install (will prompt for sudo for the setuid helper)
make install
```

Install puts:
- `/usr/local/bin/siteguard` — main binary (normal permissions)
- `/usr/local/bin/siteguard-helper` — setuid root (chown root, chmod 4755)
- `~/.config/systemd/user/siteguard-{check,reset}.{service,timer}`
- `~/.local/share/applications/siteguard.desktop`

## Usage

**GUI:**
```bash
siteguard
```

Add sites with `+ Add`. Set budget in minutes. Hit the refresh button or
wait — the UI polls ActivityWatch every 60 seconds automatically.

**CLI (also used by systemd):**
```bash
siteguard --check   # query AW, enforce any budgets that are over
siteguard --reset   # unblock all sites + zero counters (midnight reset)
```

## Config file

`~/.config/siteguard/config.conf` is human-editable:

```ini
# format: domain=budget_seconds
youtube.com=1800
reddit.com=1200
twitter.com=900
```

Budgets are in seconds. 1800 = 30 minutes.

## How blocking works

When a budget is exceeded:
1. `siteguard-helper block youtube.com` appends to `/etc/hosts`:
   ```
   127.0.0.1 youtube.com www.youtube.com # siteguard-block
   ```
2. All browsers (and curl, etc.) resolve the domain to localhost.
3. To bypass: edit `/etc/hosts` manually as root — intentionally annoying.

At midnight, `siteguard --reset` calls `siteguard-helper unblock <domain>`
for each site, which rewrites `/etc/hosts` without the marked lines.

## Security notes

- `siteguard-helper` validates domains: only `[a-zA-Z0-9.-]` accepted.
- No shell is exec'd — direct `fopen`/`fputs` to `/etc/hosts`.
- Unblock uses atomic `rename(tmp, /etc/hosts)` to avoid partial writes.
- The main binary has no elevated privileges — only the tiny helper does.

## Uninstall

```bash
make uninstall
```

Config at `~/.config/siteguard/` is preserved. Delete manually if desired.
