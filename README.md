# Squeekboard — floating keyboard for Raspberry Pi touchscreens

A floating edition of [Raspberry Pi Squeekboard](https://github.com/raspberrypi-ui/squeekboard), based on Squeekboard 1.21. Designed for Raspberry Pi OS / Debian Bookworm ARM64 with **labwc on Wayland**.

- Drag the top handle to move the keyboard; its position survives hide/show until restart.
- Compact panel with opaque keys over a translucent gray background. Apps remain clickable outside it, and fullscreen apps stay fullscreen.
- Close with **×**, then tap a text field to reopen. Focus settling reduces hide/show flicker.
- Includes a wide US URL layout with slash and period keys.

## Build and install

On the Pi, enable your distribution's matching source repositories (`deb-src`), then:

```sh
sudo apt update
sudo apt install git build-essential devscripts
git clone --branch floating-keyboard https://github.com/robertkovax/squeekboard.git
cd squeekboard
sudo apt-get build-dep .
DEB_BUILD_OPTIONS=parallel=1 CARGO_BUILD_JOBS=1 dpkg-buildpackage -b -us -uc -j1
sudo apt install ../squeekboard_1.21.0-1+rpt11+floating1_arm64.deb
```

This replaces the distribution's `squeekboard` package. Keep its previous package or use your distribution's package manager to restore that version. Future distribution upgrades may replace this build.

Run `squeekboard` inside your Wayland desktop, or log out and back in if your desktop already starts it. Keep **one keyboard instance**: replace an existing startup command rather than adding another. Tap an editable field; drag the handle to reposition; use × to dismiss. Normal layouts and language settings follow upstream Squeekboard. If needed, enable it with `gsettings set org.gnome.desktop.a11y.applications screen-keyboard-enabled true`.

For Chromium, use its native Wayland input path: `chromium --ozone-platform=wayland --enable-wayland-ime --wayland-text-input-version=3`. Apply flags through your desktop launcher if it already starts Chromium.

## Compatibility and fullscreen gestures

Requires compositor support for Wayland layer-shell, virtual keyboard, `input-method-v2` and text-input; the focus-dismiss behavior needs **layer-shell v4**. This is a Wayland keyboard; X11, other compositors, distributions and architectures have not been validated for this edition. Application text-input support is also required.

An optional [native fullscreen helper](contrib/fullscreen/README.md) lets a gesture toggle the active window without simulated F11 key events. It addresses the fullscreen-exit path that caused Chromium page fields to stop accepting keyboard input in our setup. It is separate from the keyboard; install it only if you use such gestures.

## Source and licensing

Original history, authors and component notices are retained. See [COPYING](COPYING), [AUTHORS](AUTHORS), [debian/copyright](debian/copyright), and [CONTRIBUTION-NOTICE.md](CONTRIBUTION-NOTICE.md). The original project README is [README.upstream.md](README.upstream.md).
