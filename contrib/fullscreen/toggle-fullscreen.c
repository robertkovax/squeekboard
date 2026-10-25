/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 robertkovax
 * Local fullscreen gesture client.
 * Uses the upstream wlr foreign-toplevel-management protocol, whose original
 * copyright (Ilia Bozhinov, 2018) and permission notice remain in the XML and
 * generated protocol files. This client sends no keyboard or pointer events.
 */
#include <wayland-client.h>
#include "foreign-toplevel-client.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define UNUSED __attribute__((unused))
struct window { struct window *next; struct zwlr_foreign_toplevel_handle_v1 *handle; bool active, fullscreen, closed; };
static struct window *windows;
static struct zwlr_foreign_toplevel_manager_v1 *manager;
static void title(void *d UNUSED, struct zwlr_foreign_toplevel_handle_v1 *h UNUSED,const char *s UNUSED) {}
static void app(void *d UNUSED, struct zwlr_foreign_toplevel_handle_v1 *h UNUSED,const char *s UNUSED) {}
static void output_enter(void *d UNUSED,struct zwlr_foreign_toplevel_handle_v1 *h UNUSED,struct wl_output *o UNUSED) {}
static void output_leave(void *d UNUSED,struct zwlr_foreign_toplevel_handle_v1 *h UNUSED,struct wl_output *o UNUSED) {}
static void state(void *data,struct zwlr_foreign_toplevel_handle_v1 *h UNUSED,struct wl_array *states) {
 struct window *w=data;uint32_t *s;w->active=false;w->fullscreen=false;
 wl_array_for_each(s,states) {
  if(*s==ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)w->active=true;
  if(*s==ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN)w->fullscreen=true;
 }
}
static void done(void *d UNUSED,struct zwlr_foreign_toplevel_handle_v1 *h UNUSED) {}
static void closed(void *data,struct zwlr_foreign_toplevel_handle_v1 *h UNUSED) { ((struct window *)data)->closed=true; }
static void parent(void *d UNUSED,struct zwlr_foreign_toplevel_handle_v1 *h UNUSED,struct zwlr_foreign_toplevel_handle_v1 *p UNUSED) {}
static const struct zwlr_foreign_toplevel_handle_v1_listener handle_listener={.title=title,.app_id=app,.output_enter=output_enter,.output_leave=output_leave,.state=state,.done=done,.closed=closed,.parent=parent};
static void toplevel(void *data UNUSED,struct zwlr_foreign_toplevel_manager_v1 *m UNUSED,struct zwlr_foreign_toplevel_handle_v1 *h) {
 struct window *w=calloc(1,sizeof *w);if(!w){perror("calloc");exit(1);}
 w->handle=h;w->next=windows;windows=w;zwlr_foreign_toplevel_handle_v1_add_listener(h,&handle_listener,w);
}
static void finished(void *data UNUSED,struct zwlr_foreign_toplevel_manager_v1 *m UNUSED) {}
static const struct zwlr_foreign_toplevel_manager_v1_listener manager_listener={.toplevel=toplevel,.finished=finished};
static void global(void *data UNUSED,struct wl_registry *r,uint32_t name,const char *interface,uint32_t version) {
 if(!strcmp(interface,"zwlr_foreign_toplevel_manager_v1") && version>=2) {
  manager=wl_registry_bind(r,name,&zwlr_foreign_toplevel_manager_v1_interface,version<3?version:3);
  zwlr_foreign_toplevel_manager_v1_add_listener(manager,&manager_listener,NULL);
 }
}
static void removed(void *d UNUSED,struct wl_registry *r UNUSED,uint32_t name UNUSED) {}
static const struct wl_registry_listener registry_listener={.global=global,.global_remove=removed};
int main(int argc,char **argv) {
 bool status=argc==2 && !strcmp(argv[1],"--status");
 if(argc>2 || (argc==2 && !status)){fprintf(stderr,"Usage: %s [--status]\n",argv[0]);return 2;}
 alarm(3);
 struct wl_display *display=wl_display_connect(NULL);
 if(!display){fprintf(stderr,"MODEP fullscreen: no Wayland display\n");return 1;}
 struct wl_registry *r=wl_display_get_registry(display);wl_registry_add_listener(r,&registry_listener,NULL);
 if(wl_display_roundtrip(display)<0 || !manager || wl_display_roundtrip(display)<0){fprintf(stderr,"MODEP fullscreen: toplevel management unavailable\n");wl_display_disconnect(display);return 1;}
 struct window *active=NULL;unsigned count=0;
 for(struct window *w=windows;w;w=w->next)if(w->active && !w->closed){active=w;count++;}
 int result=0;
 if(count!=1){fprintf(stderr,"MODEP fullscreen: expected one active window, found %u\n",count);result=1;}
 else if(status)puts(active->fullscreen?"fullscreen":"windowed");
 else {
  if(active->fullscreen)zwlr_foreign_toplevel_handle_v1_unset_fullscreen(active->handle);
  else zwlr_foreign_toplevel_handle_v1_set_fullscreen(active->handle,NULL);
  if(wl_display_roundtrip(display)<0)result=1;
 }
 while(windows){struct window *w=windows;windows=w->next;zwlr_foreign_toplevel_handle_v1_destroy(w->handle);free(w);}
 zwlr_foreign_toplevel_manager_v1_destroy(manager);wl_registry_destroy(r);wl_display_flush(display);wl_display_disconnect(display);return result;
}
