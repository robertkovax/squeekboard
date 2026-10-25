#!/bin/sh
# Build the local client against the bundled, unmodified upstream protocol XML.
set -eu
cd "$(dirname "$0")"
mkdir -p build
wayland-scanner client-header wlr-foreign-toplevel-management-unstable-v1.xml build/foreign-toplevel-client.h
wayland-scanner private-code wlr-foreign-toplevel-management-unstable-v1.xml build/foreign-toplevel-protocol.c
cc -Wall -Wextra -Werror -O2 -Ibuild -o build/toggle-fullscreen toggle-fullscreen.c build/foreign-toplevel-protocol.c $(pkg-config --cflags --libs wayland-client)
