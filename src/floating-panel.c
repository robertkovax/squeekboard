/* Local MODEP floating panel extension to Squeekboard.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Original Squeekboard/Purism/Raspberry Pi copyright and license notices are
 * retained in the surrounding project and debian/copyright.
 */
#include "floating-panel.h"
#include "main.h"
#include "wayland.h"
#include <math.h>

#define PANEL_DATA "modep-floating-panel"
#define HANDLE_HEIGHT 38
#define EDGE_GAP 12

/* The layer surface stays stationary and transparent. Only the panel's input
 * rectangle receives events; the rest passes through to the fullscreen app.
 * Moving a child rather than the Wayland surface also makes drag coordinates
 * stable for both pointer and touch events.
 */
struct floating_panel {
    PhoshLayerSurface *window;
    GtkWidget *fixed;
    GtkWidget *panel;
    GtkWidget *keyboard;
    GtkWidget *close;
    GtkWidget *focus_anchor;
    struct squeek_state_manager *state_manager;
    guint requested_height;
    int width, height, output_width, output_height;
    int x, y, drag_x, drag_y;
    double press_x, press_y;
    gboolean positioned;
};

static void
input_region(struct floating_panel *self)
{
    GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(self->window));
    if (!window)
        return;
    cairo_rectangle_int_t rect = {self->x, self->y, self->width, self->height};
    cairo_region_t *region = self->positioned
        ? cairo_region_create_rectangle(&rect) : cairo_region_create();
    gdk_window_input_shape_combine_region(window, region, 0, 0);
    cairo_region_destroy(region);
}

static void
move_panel(struct floating_panel *self, int x, int y)
{
    self->x = CLAMP(x, 0, MAX(0, self->output_width - self->width));
    self->y = CLAMP(y, 0, MAX(0, self->output_height - self->height));
    gtk_fixed_move(GTK_FIXED(self->fixed), self->panel, self->x, self->y);
    input_region(self);
}

static void
configure_panel(struct floating_panel *self, int width, int height)
{
    if (width <= 0 || height <= 0)
        return;
    self->output_width = width;
    self->output_height = height;
    int gap = MIN(EDGE_GAP, MIN(width, height) / 8);
    self->width = MIN(width - 2 * gap, MIN(800, MAX(300, width * 2 / 3)));
    self->width = MAX(1, self->width);
    int keyboard_height = MIN((int)self->requested_height, self->width / 3);
    keyboard_height = MAX(1, MIN(keyboard_height, height - HANDLE_HEIGHT - 2 * gap));
    self->height = MIN(height, keyboard_height + HANDLE_HEIGHT);
    gtk_widget_set_size_request(self->keyboard, self->width, keyboard_height);
    gtk_widget_set_size_request(self->panel, self->width, self->height);
    if (!self->positioned) {
        self->x = (width - self->width) / 2;
        self->y = height - self->height - gap;
        self->positioned = TRUE;
    }
    move_panel(self, self->x, self->y);
}

static void
configured(PhoshLayerSurface *surface, struct floating_panel *self)
{
    configure_panel(self, phosh_layer_surface_get_configured_width(surface),
                     phosh_layer_surface_get_configured_height(surface));
}

static gboolean
clear_background(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
    return FALSE;
}

static gboolean
event_root(GtkGesture *gesture, double *x, double *y)
{
    GdkEventSequence *sequence = gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    const GdkEvent *event = gtk_gesture_get_last_event(gesture, sequence);
    return event && gdk_event_get_root_coords(event, x, y);
}

static void
drag_begin(GtkGestureDrag *gesture, double x, double y, struct floating_panel *self)
{
    self->drag_x = self->x;
    self->drag_y = self->y;
    if (!event_root(GTK_GESTURE(gesture), &self->press_x, &self->press_y))
        return;
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
drag_update(GtkGestureDrag *gesture, double dx, double dy, struct floating_panel *self)
{
    double x, y;
    if (event_root(GTK_GESTURE(gesture), &x, &y))
        move_panel(self, self->drag_x + (int)round(x - self->press_x),
                         self->drag_y + (int)round(y - self->press_y));
}

static void
mapped(GtkWidget *widget, struct floating_panel *self)
{
    input_region(self);
    /* Touch hover can outlive unmapping without another pointer crossing. */
    gtk_widget_unset_state_flags(self->close, GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE);
}

static void
unmapped(GtkWidget *widget, struct floating_panel *self)
{
    gtk_widget_unset_state_flags(self->close, GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE);
}

/* Map this empty on-demand layer only in response to X. Labwc transfers
 * keyboard focus on mapping; layer surfaces have no taskbar entry. Keep it
 * mapped after focus leaves so that no unmap restores the old text focus
 * during the application's own pointer/touch selection handling.
 */
static void
anchor_mapped(GtkWidget *widget, gpointer data)
{
    cairo_region_t *empty = cairo_region_create();
    gdk_window_input_shape_combine_region(gtk_widget_get_window(widget), empty, 0, 0);
    cairo_region_destroy(empty);
}

static void
floating_panel_free(gpointer data)
{
    struct floating_panel *self = data;
    if (self->focus_anchor)
        gtk_widget_destroy(self->focus_anchor);
    g_free(self);
}

static void
close_clicked(GtkButton *button, struct floating_panel *self)
{
    if (!self->focus_anchor) {
        self->focus_anchor = g_object_new(PHOSH_TYPE_LAYER_SURFACE,
            "layer-shell", squeek_wayland->layer_shell,
            "width", 1, "height", 1,
            "layer", ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
            "exclusive-zone", 0,
            "kbd-interactivity", FALSE,
            "namespace", "osk-dismiss-focus", NULL);
        /* Direct setter preserves the protocol's ON_DEMAND enum (2), unlike
         * the historical boolean GObject property. Never use EXCLUSIVE. */
        phosh_layer_surface_set_kbd_interactivity(PHOSH_LAYER_SURFACE(self->focus_anchor),
            ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND);
        gtk_widget_set_app_paintable(self->focus_anchor, TRUE);
        GdkVisual *visual = gdk_screen_get_rgba_visual(gtk_widget_get_screen(self->focus_anchor));
        if (visual)
            gtk_widget_set_visual(self->focus_anchor, visual);
        gtk_style_context_add_class(gtk_widget_get_style_context(self->focus_anchor),
                                    "modep-focus-anchor");
        g_signal_connect(self->focus_anchor, "draw", G_CALLBACK(clear_background), NULL);
        g_signal_connect(self->focus_anchor, "map", G_CALLBACK(anchor_mapped), NULL);
    }
    if (gtk_window_is_active(GTK_WINDOW(self->focus_anchor)))
        return;
    gtk_widget_hide(self->focus_anchor);
    gtk_widget_show(self->focus_anchor);
}

void
floating_panel_attach(PhoshLayerSurface *window, GtkWidget *keyboard, guint keyboard_height,
                      struct squeek_state_manager *state_manager)
{
    struct floating_panel *self = g_new0(struct floating_panel, 1);
    self->window = window;
    self->keyboard = keyboard;
    self->state_manager = state_manager;
    self->requested_height = keyboard_height;
    /* Tooltip popups have no native popup parent on this custom Wayland layer
     * surface. Avoid retrying those popups during browser focus hand-offs. */
    gtk_widget_set_has_tooltip(keyboard, FALSE);
    g_object_set_data_full(G_OBJECT(window), PANEL_DATA, self, floating_panel_free);

    GtkWidget *surface = GTK_WIDGET(window);
    gtk_widget_set_app_paintable(surface, TRUE);
    GdkVisual *visual = gdk_screen_get_rgba_visual(gtk_widget_get_screen(surface));
    if (visual)
        gtk_widget_set_visual(surface, visual);
    gtk_style_context_add_class(gtk_widget_get_style_context(surface), "modep-floating-surface");

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        ".modep-floating-surface, .modep-focus-anchor { background-color: transparent; box-shadow: none; border: 0; }"
        ".modep-floating-panel { background-color: transparent; }"
        ".modep-floating-handle { background-color: rgba(68,68,68,0.80); color: #eeeeee; }"
        ".modep-floating-close { border: 0; border-radius: 0; padding: 0;"
        " min-width: 52px; min-height: 38px; background: transparent;"
        " color: #eeeeee; box-shadow: none; }"
        ".modep-floating-close:hover { background: transparent; }"
        ".modep-floating-close:active { background: rgba(255,255,255,0.15); }"
        ".modep-floating-close label { font-size: 26px; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(surface),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    self->fixed = gtk_fixed_new();
    self->panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(self->panel), "modep-floating-panel");
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(header), "modep-floating-handle");
    GtkWidget *handle = gtk_event_box_new();
    gtk_widget_set_size_request(handle, -1, HANDLE_HEIGHT);
    gtk_event_box_set_above_child(GTK_EVENT_BOX(handle), TRUE);
    gtk_container_add(GTK_CONTAINER(handle), gtk_label_new("\342\211\241  Move keyboard"));
    gtk_widget_set_can_focus(handle, FALSE);
    gtk_widget_add_events(handle, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                         GDK_POINTER_MOTION_MASK | GDK_TOUCH_MASK);
    GtkGesture *drag = gtk_gesture_drag_new(handle);
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
    g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_begin), self);
    g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update), self);
    g_object_set_data_full(G_OBJECT(handle), "modep-drag-gesture", drag, g_object_unref);
    GtkWidget *close = gtk_button_new_with_label("\303\227");
    self->close = close;
    gtk_style_context_add_class(gtk_widget_get_style_context(close), "modep-floating-close");
    gtk_button_set_relief(GTK_BUTTON(close), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(close, FALSE);
    gtk_widget_set_focus_on_click(close, FALSE);
    atk_object_set_name(gtk_widget_get_accessible(close), "Dismiss keyboard and release text focus");
    g_signal_connect(close, "clicked", G_CALLBACK(close_clicked), self);
    gtk_box_pack_start(GTK_BOX(header), handle, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header), close, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(self->panel), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(self->panel), keyboard, TRUE, TRUE, 0);
    gtk_fixed_put(GTK_FIXED(self->fixed), self->panel, 0, 0);
    gtk_container_add(GTK_CONTAINER(window), self->fixed);
    g_signal_connect(window, "draw", G_CALLBACK(clear_background), NULL);
    g_signal_connect(window, "configured", G_CALLBACK(configured), self);
    g_signal_connect(window, "map", G_CALLBACK(mapped), self);
    g_signal_connect(window, "unmap", G_CALLBACK(unmapped), self);
    gtk_widget_show_all(self->fixed);
}

void
floating_panel_resize(PhoshLayerSurface *window, guint keyboard_height)
{
    struct floating_panel *self = g_object_get_data(G_OBJECT(window), PANEL_DATA);
    if (self) {
        self->requested_height = keyboard_height;
        configured(window, self);
    }
}

void
floating_panel_hide(PhoshLayerSurface *window)
{
    gtk_widget_hide(GTK_WIDGET(window));
}
