#
# Makefile for example dynamic plugins
# Build each plugin as a .so file using the typed plugin API.
#
# Usage:
#   make          — build all plugin .so files
#   make install  — copy .so + .yaml to ~/.hermes/plugins/
#   make clean    — remove .so files
#

CC       = gcc
CFLAGS   = -O2 -fPIC -Wall -Wextra -I ../../include -I ../../lib/libplugin
LDFLAGS  = -shared -ldl

PLUGIN_SRC = plugin_honcho.c plugin_kanban.c plugin_spotify.c
PLUGIN_SO  = plugin_honcho.so plugin_kanban.so plugin_spotify.so
YAML_FILES = honcho-memory.yaml kanban-board.yaml spotify-control.yaml

.PHONY: all install clean

all: $(PLUGIN_SO)

plugin_honcho.so: plugin_honcho.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

plugin_kanban.so: plugin_kanban.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

plugin_spotify.so: plugin_spotify.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

install: all
	@mkdir -p ~/.hermes/plugins
	cp $(PLUGIN_SO) ~/.hermes/plugins/
	cp $(YAML_FILES) ~/.hermes/plugins/
	@echo "Plugins installed to ~/.hermes/plugins/"
	@echo "Add to your config.yaml:"
	@echo "  plugin:"
	@echo "    dirs: \"~/.hermes/plugins\""
	@echo "    enabled: \"honcho-memory,kanban-board,spotify-control\""

clean:
	rm -f $(PLUGIN_SO)
