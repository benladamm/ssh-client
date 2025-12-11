#ifndef TERMINAL_VIEW_H
#define TERMINAL_VIEW_H

#include <gtk/gtk.h>

#include "db.h"

// Crée la vue du terminal
GtkWidget* create_terminal_view();

// Connecte le terminal à un hôte
void terminal_view_connect(GtkWidget *view, const char *hostname, int port, const char *username, const char *password, const char *key_path, const char *protocol, Host *proxy_host);

#endif
