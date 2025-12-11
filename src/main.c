#include <gtk/gtk.h>
#include <libssh/libssh.h>
#include <signal.h>
#include "window.h"
#include "db.h"
#include "theme_manager.h"

static void activate(GtkApplication *app, gpointer user_data) {
    theme_manager_init();
    
    if (!db_init()) {
        g_printerr("Database initialization error.\n");
    }

    GtkWidget *window = create_main_window(app);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    ssh_init();
    
    signal(SIGPIPE, SIG_IGN);

    GtkApplication *app;
    int status;

    app = gtk_application_new("com.benladamm.modernssh", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    
    db_close();
    g_object_unref(app);
    
    ssh_finalize();

    return status;
}
