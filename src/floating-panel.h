/* Local MODEP extension to Squeekboard, under its GPL-3.0-or-later terms.
 * Original Squeekboard, Purism and Raspberry Pi notices remain in the source.
 */
#pragma once
#include "eek/layersurface.h"

struct squeek_state_manager;
void floating_panel_attach(PhoshLayerSurface *window, GtkWidget *keyboard,
                           guint keyboard_height, struct squeek_state_manager *state_manager);
void floating_panel_resize(PhoshLayerSurface *window, guint keyboard_height);

void floating_panel_hide(PhoshLayerSurface *window);
