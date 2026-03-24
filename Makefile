CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -Isrc \
          $(shell pkg-config --cflags gtk+-3.0 libcurl json-c)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 libcurl json-c)

SRC     = src/main.c src/ui.c src/aw_client.c src/blocker.c src/config.c
TARGET  = siteguard
HELPER  = siteguard-helper

SYSTEMD_USER_DIR = $(HOME)/.config/systemd/user

# ---- Default target ----

all: $(TARGET) $(HELPER)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(HELPER): helper/helper.c
	$(CC) -Wall -Wextra -O2 -o $@ $^

# ---- Dependency check (run before building) ----

deps:
	@echo "Checking dependencies..."
	@pkg-config gtk+-3.0  || (echo "MISSING: sudo pacman -S gtk3"    && exit 1)
	@pkg-config libcurl   || (echo "MISSING: sudo pacman -S curl"    && exit 1)
	@pkg-config json-c    || (echo "MISSING: sudo pacman -S json-c"  && exit 1)
	@which activitywatch  || echo "NOTE: Install ActivityWatch from AUR: yay -S activitywatch-bin"
	@echo "Also install browser watcher: yay -S aw-watcher-web-firefox (or chromium variant)"
	@echo "All OK."

# ---- Install ----
#
# The helper MUST be installed setuid root so it can write /etc/hosts.
# The main binary is installed as a normal executable.

install: all
	@echo "Installing $(TARGET) → /usr/local/bin/"
	install -Dm755 $(TARGET) /usr/local/bin/$(TARGET)

	@echo "Installing $(HELPER) as setuid root → /usr/local/bin/"
	@echo "(requires sudo)"
	sudo install -Dm755 $(HELPER) /usr/local/bin/$(HELPER)
	sudo chown root:root /usr/local/bin/$(HELPER)
	sudo chmod 4755      /usr/local/bin/$(HELPER)

	@echo "Installing systemd user timers..."
	mkdir -p $(SYSTEMD_USER_DIR)
	install -Dm644 data/siteguard-check.service $(SYSTEMD_USER_DIR)/
	install -Dm644 data/siteguard-check.timer   $(SYSTEMD_USER_DIR)/
	install -Dm644 data/siteguard-reset.service $(SYSTEMD_USER_DIR)/
	install -Dm644 data/siteguard-reset.timer   $(SYSTEMD_USER_DIR)/
	systemctl --user daemon-reload
	systemctl --user enable --now siteguard-check.timer
	systemctl --user enable --now siteguard-reset.timer

	@echo "Installing desktop entry..."
	install -Dm644 data/siteguard.desktop \
		$(HOME)/.local/share/applications/siteguard.desktop
	update-desktop-database $(HOME)/.local/share/applications/ 2>/dev/null || true

	@echo ""
	@echo "Done. Run 'siteguard' to open the UI."

# ---- Uninstall ----

uninstall:
	-systemctl --user disable --now siteguard-check.timer
	-systemctl --user disable --now siteguard-reset.timer
	sudo rm -f /usr/local/bin/$(TARGET) /usr/local/bin/$(HELPER)
	rm -f $(SYSTEMD_USER_DIR)/siteguard-check.{service,timer}
	rm -f $(SYSTEMD_USER_DIR)/siteguard-reset.{service,timer}
	systemctl --user daemon-reload
	@echo "Uninstalled. Config preserved at ~/.config/siteguard/"

# ---- Utility ----

clean:
	rm -f $(TARGET) $(HELPER)

status:
	systemctl --user status siteguard-check.timer siteguard-reset.timer

.PHONY: all deps install uninstall clean status
