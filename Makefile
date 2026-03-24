CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -Isrc \
          $(shell pkg-config --cflags gtk+-3.0 libcurl json-c)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 libcurl json-c)

SRC     = src/main.c src/ui.c src/aw_client.c src/blocker.c src/config.c
TARGET  = siteguard
HELPER  = siteguard-helper

SYSTEMD_USER_DIR = $(HOME)/.config/systemd/user

all: $(TARGET) $(HELPER)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(HELPER): helper/helper.c
	$(CC) -Wall -Wextra -O2 -o $@ $^

deps:
	@echo "Checking dependencies..."
	@pkg-config gtk+-3.0  || (echo "MISSING: sudo pacman -S gtk3"    && exit 1)
	@pkg-config libcurl   || (echo "MISSING: sudo pacman -S curl"    && exit 1)
	@pkg-config json-c    || (echo "MISSING: sudo pacman -S json-c"  && exit 1)
	@echo "All dependencies satisfied."

install: all
	@echo "Installing $(TARGET) and $(HELPER)..."
	sudo install -Dm755 $(TARGET) /usr/local/bin/$(TARGET)
	sudo install -Dm755 $(HELPER) /usr/local/bin/$(HELPER)
	sudo chown root:root /usr/local/bin/$(HELPER)
	sudo chmod 4755      /usr/local/bin/$(HELPER)

	@echo "Installing systemd timers..."
	mkdir -p $(SYSTEMD_USER_DIR)
	install -Dm644 data/siteguard-check.service $(SYSTEMD_USER_DIR)/
	install -Dm644 data/siteguard-check.timer   $(SYSTEMD_USER_DIR)/
	install -Dm644 data/siteguard-reset.service $(SYSTEMD_USER_DIR)/
	install -Dm644 data/siteguard-reset.timer   $(SYSTEMD_USER_DIR)/
	-systemctl --user daemon-reload 2>/dev/null || true
	-systemctl --user enable --now siteguard-check.timer 2>/dev/null || true
	-systemctl --user enable --now siteguard-reset.timer 2>/dev/null || true

	@echo "Installing desktop entry..."
	install -Dm644 data/siteguard.desktop \
		$(HOME)/.local/share/applications/siteguard.desktop

	@echo ""
	@echo "Done! Run 'siteguard' to open the UI."

uninstall:
	-systemctl --user disable --now siteguard-check.timer 2>/dev/null || true
	-systemctl --user disable --now siteguard-reset.timer 2>/dev/null || true
	sudo rm -f /usr/local/bin/$(TARGET) /usr/local/bin/$(HELPER)
	rm -f $(SYSTEMD_USER_DIR)/siteguard-check.{service,timer}
	rm -f $(SYSTEMD_USER_DIR)/siteguard-reset.{service,timer}
	-systemctl --user daemon-reload 2>/dev/null || true
	@echo "Uninstalled."

clean:
	rm -f $(TARGET) $(HELPER)

status:
	@which $(TARGET) >/dev/null 2>&1 && echo "Installed: yes" || echo "Installed: no"
	@systemctl --user is-active siteguard-check.timer 2>/dev/null && echo "Timer: active" || echo "Timer: inactive"

.PHONY: all deps install uninstall clean status
