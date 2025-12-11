#ifndef SFTP_VIEW_H
#define SFTP_VIEW_H

#include <gtk/gtk.h>
#include "db.h"

// Create complete SFTP view (with navigation, file list, etc.)
GtkWidget* create_sftp_view();

// Connect SFTP view to a host
void sftp_view_connect(GtkWidget *view, Host *host);

#endif
