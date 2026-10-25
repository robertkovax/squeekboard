# Optional native fullscreen gesture helper

For labwc or another compositor exposing `wlr-foreign-toplevel-management` v2+.
It toggles the active window through Wayland, without simulated keys or pointer
events. This avoids the simulated-F11 path that disrupted Chromium page typing
after fullscreen exit in our touchscreen setup. It does not change browser code.

```sh
sudo apt install build-essential pkg-config libwayland-dev
./contrib/fullscreen/build.sh
mkdir -p "$HOME/.local/bin"
install -m755 contrib/fullscreen/build/toggle-fullscreen "$HOME/.local/bin/toggle-fullscreen"
```

Run `toggle-fullscreen --status` inside the Wayland session to inspect the active
window, or `toggle-fullscreen` to toggle it. Point your existing swipe binding at
the absolute path to this command instead of an F11 key injector. Keep just one
gesture handler for that swipe; remove any duplicate startup command/service.
Configure the gesture's device and direction for your own display. No gesture
service is installed automatically.

The local client is GPL-3.0-or-later. The unmodified protocol XML retains
Copyright © 2018 Ilia Bozhinov and its full permission/disclaimer notice.
