#include "terminal_view.h"
#include "ssh_backend.h"
#include "db.h"
#include <vte/vte.h>

typedef struct {
    GtkWidget *terminal;
    SSHContext *ssh_ctx;
    guint poll_id;
    GtkWidget *box;
    GtkWidget *notebook;
} TerminalTab;

static void on_terminal_commit(VteTerminal *terminal, gchar *text, guint size, gpointer data) {
    TerminalTab *tab = (TerminalTab *)data;
    if (tab->ssh_ctx && tab->ssh_ctx->is_connected) {
        ssh_write_data(tab->ssh_ctx, text, size);
    }
}

static gboolean on_ssh_poll(gpointer data) {
    TerminalTab *tab = (TerminalTab *)data;
    
    if (!tab->ssh_ctx) return FALSE;

    ssh_proxy_poll(tab->ssh_ctx);

    if (!tab->ssh_ctx->is_connected || !ssh_is_channel_open(tab->ssh_ctx)) {
        GtkWidget *notebook = tab->notebook;
        GtkWidget *page = tab->box;
        
        int page_num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), page);
        if (page_num != -1) {
            gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), page_num);
        }
        return FALSE;
    }

    char buffer[4096];
    int nbytes = ssh_read_nonblocking(tab->ssh_ctx, buffer, sizeof(buffer));
    if (nbytes > 0) {
        vte_terminal_feed(VTE_TERMINAL(tab->terminal), buffer, nbytes);
    }
    return TRUE;
}

static void on_tab_destroy(GtkWidget *widget, gpointer data) {
    TerminalTab *tab = (TerminalTab *)data;
    if (tab->poll_id > 0) {
        g_source_remove(tab->poll_id);
    }
    if (tab->ssh_ctx) {
        ssh_context_free(tab->ssh_ctx);
    }
    g_free(tab);
}

static void on_close_tab_clicked(GtkWidget *btn, gpointer user_data) {
    GtkWidget *page = g_object_get_data(G_OBJECT(btn), "page_widget");
    GtkNotebook *notebook = GTK_NOTEBOOK(g_object_get_data(G_OBJECT(btn), "notebook"));
    
    int page_num = gtk_notebook_page_num(notebook, page);
    if (page_num != -1) {
        gtk_notebook_remove_page(notebook, page_num);
    }
}

GtkWidget* create_terminal_view() {
    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook), FALSE);
    return notebook;
}

typedef struct {
    TerminalTab *tab;
    GtkWidget *box_ptr; // Weak pointer
    SSHContext *ssh_ctx;
    char *hostname;
    int port;
    char *username;
    char *password;
    char *key_path;
    char *proxy_hostname;
    int proxy_port;
    char *proxy_user;
    char *proxy_password;
    char *proxy_key_path;
    int result;
} ConnectionData;

static gboolean on_connection_complete(gpointer data) {
    ConnectionData *cd = (ConnectionData *)data;
    
    if (cd->box_ptr == NULL) {
        if (cd->ssh_ctx) {
            ssh_context_free(cd->ssh_ctx);
        }
    } else {
        TerminalTab *tab = cd->tab;
        tab->ssh_ctx = cd->ssh_ctx;
        g_object_remove_weak_pointer(G_OBJECT(tab->box), (gpointer *)&cd->box_ptr);

        if (cd->result == 0) {
            vte_terminal_feed(VTE_TERMINAL(tab->terminal), "Connected.\r\n", -1);
            tab->poll_id = g_timeout_add(10, on_ssh_poll, tab);
            gtk_widget_grab_focus(tab->terminal);
        } else {
            char msg[512];
            snprintf(msg, sizeof(msg), "Connection failed: %s\r\n", ssh_get_error_msg(tab->ssh_ctx));
            vte_terminal_feed(VTE_TERMINAL(tab->terminal), msg, -1);
        }
    }

    g_free(cd->hostname);
    g_free(cd->username);
    g_free(cd->password);
    g_free(cd->key_path);
    g_free(cd->proxy_hostname);
    g_free(cd->proxy_user);
    g_free(cd->proxy_password);
    g_free(cd->proxy_key_path);
    g_free(cd);
    
    return FALSE;
}

static gpointer connection_thread_func(gpointer data) {
    ConnectionData *cd = (ConnectionData *)data;
    
    cd->result = ssh_connect_to_server(cd->ssh_ctx, 
        cd->hostname, cd->port, cd->username, cd->password, cd->key_path,
        cd->proxy_hostname, cd->proxy_port, cd->proxy_user, cd->proxy_password, cd->proxy_key_path,
        true);

    g_idle_add(on_connection_complete, cd);
    return NULL;
}

void terminal_view_connect(GtkWidget *view, const char *hostname, int port, const char *username, const char *password, const char *key_path, const char *protocol, Host *proxy_host) {
    GtkNotebook *notebook = GTK_NOTEBOOK(view);
    
    TerminalTab *tab = g_new0(TerminalTab, 1);
    tab->notebook = view;
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    tab->box = box;
    
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(box), scrolled);

    tab->terminal = vte_terminal_new();
    gtk_widget_set_hexpand(tab->terminal, TRUE);
    gtk_widget_set_vexpand(tab->terminal, TRUE);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(tab->terminal), TRUE);
    
    GdkRGBA fg = {0.8, 0.84, 0.96, 1.0};
    GdkRGBA bg = {0.12, 0.12, 0.18, 1.0};
    vte_terminal_set_colors(VTE_TERMINAL(tab->terminal), &fg, &bg, NULL, 0);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), tab->terminal);

    g_signal_connect(tab->terminal, "commit", G_CALLBACK(on_terminal_commit), tab);
    g_signal_connect(box, "destroy", G_CALLBACK(on_tab_destroy), tab);
    
    GtkWidget *label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *label = gtk_label_new(hostname);
    GtkWidget *close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_set_focusable(close_btn, FALSE);
    gtk_button_set_has_frame(GTK_BUTTON(close_btn), FALSE);
    
    gtk_box_append(GTK_BOX(label_box), label);
    gtk_box_append(GTK_BOX(label_box), close_btn);
    
    g_object_set_data(G_OBJECT(close_btn), "page_widget", box);
    g_object_set_data(G_OBJECT(close_btn), "notebook", notebook);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_close_tab_clicked), NULL);
    
    int page_num = gtk_notebook_append_page(notebook, box, label_box);
    gtk_notebook_set_tab_reorderable(notebook, box, TRUE);
    gtk_notebook_set_current_page(notebook, page_num);

    db_add_history(hostname, username, protocol ? protocol : "ssh");

    if (g_strcmp0(protocol, "telnet") == 0 || g_strcmp0(protocol, "ftp") == 0 || g_strcmp0(protocol, "sftp") == 0) {
        char *argv[10] = {NULL};
        int arg_idx = 0;
        
        if (g_strcmp0(protocol, "telnet") == 0) {
            argv[arg_idx++] = g_strdup("telnet");
            argv[arg_idx++] = g_strdup(hostname);
            char port_str[16];
            sprintf(port_str, "%d", port);
            argv[arg_idx++] = g_strdup(port_str);
        } else if (g_strcmp0(protocol, "ftp") == 0) {
            argv[arg_idx++] = g_strdup("ftp");
            argv[arg_idx++] = g_strdup(hostname);
        } else { // sftp
            argv[arg_idx++] = g_strdup("sftp");
            if (port != 22) {
                argv[arg_idx++] = g_strdup("-P");
                char port_str[16];
                sprintf(port_str, "%d", port);
                argv[arg_idx++] = g_strdup(port_str);
            }
            char *conn_str = g_strdup_printf("%s@%s", username, hostname);
            argv[arg_idx++] = conn_str;
        }
        
        vte_terminal_spawn_async(VTE_TERMINAL(tab->terminal),
            VTE_PTY_DEFAULT,
            NULL, // working dir
            argv,
            NULL, // env
            G_SPAWN_SEARCH_PATH,
            NULL, // child setup
            NULL, // child setup data
            NULL, // child setup destroy
            -1,   // timeout
            NULL, // cancellable
            NULL, // callback
            NULL  // user_data
        );
        
        for(int j=0; j<arg_idx; j++) {
            g_free(argv[j]);
        }
        
        gtk_widget_grab_focus(tab->terminal);
        
    } else {
        // SSH (default)
        SSHContext *ctx = ssh_context_new();
        
        char msg[256];
        if (proxy_host) {
            snprintf(msg, sizeof(msg), "Connecting to %s@%s:%d via %s...\r\n", username, hostname, port, proxy_host->hostname);
        } else {
            snprintf(msg, sizeof(msg), "Connecting to %s@%s:%d...\r\n", username, hostname, port);
        }
        vte_terminal_feed(VTE_TERMINAL(tab->terminal), msg, -1);

        // Prepare connection data
        ConnectionData *cd = g_new0(ConnectionData, 1);
        cd->tab = tab;
        cd->box_ptr = tab->box;
        cd->ssh_ctx = ctx;
        
        g_object_add_weak_pointer(G_OBJECT(tab->box), (gpointer *)&cd->box_ptr);

        cd->hostname = g_strdup(hostname);
        cd->port = port;
        cd->username = g_strdup(username);
        cd->password = g_strdup(password);
        cd->key_path = g_strdup(key_path);
        
        if (proxy_host) {
            cd->proxy_hostname = g_strdup(proxy_host->hostname);
            cd->proxy_port = proxy_host->port;
            cd->proxy_user = g_strdup(proxy_host->username);
            cd->proxy_password = g_strdup(proxy_host->password);
            cd->proxy_key_path = g_strdup(proxy_host->key_path);
        }

        // Start connection thread
        GThread *thread = g_thread_new("ssh-connect", connection_thread_func, cd);
        g_thread_unref(thread);
    }
}
