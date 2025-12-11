#include "sftp_view.h"
#include "ssh_sftp.h"
#include "ssh_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    GtkWidget *box;
    GtkWidget *address_bar;
    GtkWidget *tree_view;
    GtkListStore *list_store;
    GtkWidget *status_bar;
    
    SSHContext *ssh_ctx;
    SFTPContext *sftp_ctx;
    char *current_path;
} SFTPViewData;

enum {
    COL_ICON = 0,
    COL_NAME,
    COL_SIZE,
    COL_PERMS,
    COL_IS_DIR,
    NUM_COLS
};

static void update_file_list(SFTPViewData *data, const char *path);

static void on_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    SFTPViewData *data = (SFTPViewData *)user_data;
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(data->list_store), &iter, path)) {
        gboolean is_dir;
        char *name;
        gtk_tree_model_get(GTK_TREE_MODEL(data->list_store), &iter, COL_IS_DIR, &is_dir, COL_NAME, &name, -1);
        
        if (is_dir) {
            char new_path[1024];
            if (g_strcmp0(name, "..") == 0) {
                if (g_strcmp0(data->current_path, "/") != 0) {
                    char *last_slash = strrchr(data->current_path, '/');
                    if (last_slash && last_slash != data->current_path) {
                        *last_slash = '\0';
                        strcpy(new_path, data->current_path);
                    } else {
                        strcpy(new_path, "/");
                    }
                } else {
                    strcpy(new_path, "/");
                }
            } else {
                if (g_strcmp0(data->current_path, "/") == 0) {
                    snprintf(new_path, sizeof(new_path), "/%s", name);
                } else {
                    snprintf(new_path, sizeof(new_path), "%s/%s", data->current_path, name);
                }
            }
            update_file_list(data, new_path);
        }
        g_free(name);
    }
}

static void update_file_list(SFTPViewData *data, const char *path) {
    gtk_list_store_clear(data->list_store);

    if (!data->sftp_ctx || !data->sftp_ctx->is_initialized) {
        if (data->current_path) {
            g_free(data->current_path);
            data->current_path = NULL;
        }
        gtk_editable_set_text(GTK_EDITABLE(data->address_bar), "");
        return;
    }

    int count = 0;
    SFTPFile **files = sftp_list_directory(data->sftp_ctx, path, &count);
    
    if (!files && count == 0) {
        return;
    }
    
    if (g_strcmp0(path, "/") != 0) {
        GtkTreeIter iter;
        gtk_list_store_append(data->list_store, &iter);
        gtk_list_store_set(data->list_store, &iter, 
                           COL_ICON, "go-up-symbolic",
                           COL_NAME, "..", 
                           COL_SIZE, "", 
                           COL_PERMS, "", 
                           COL_IS_DIR, TRUE,
                           -1);
    }

    for (int i = 0; i < count; i++) {
        if (g_strcmp0(files[i]->name, ".") == 0 || g_strcmp0(files[i]->name, "..") == 0) continue;
        
        GtkTreeIter iter;
        gtk_list_store_append(data->list_store, &iter);
        
        char size_str[32];
        if (files[i]->type == SFTP_TYPE_DIRECTORY) {
            strcpy(size_str, "<DIR>");
        } else {
             if (files[i]->size < 1024) sprintf(size_str, "%lu B", files[i]->size);
             else if (files[i]->size < 1024*1024) sprintf(size_str, "%.1f KB", files[i]->size / 1024.0);
             else sprintf(size_str, "%.1f MB", files[i]->size / (1024.0*1024.0));
        }

        const char *icon = "text-x-generic-symbolic";
        if (files[i]->type == SFTP_TYPE_DIRECTORY) icon = "folder-symbolic";
        
        gtk_list_store_set(data->list_store, &iter,
                           COL_ICON, icon,
                           COL_NAME, files[i]->name,
                           COL_SIZE, size_str,
                           COL_PERMS, files[i]->permissions,
                           COL_IS_DIR, files[i]->type == SFTP_TYPE_DIRECTORY,
                           -1);
    }
    
    sftp_free_file_list(files, count);
    
    if (data->current_path) g_free(data->current_path);
    data->current_path = g_strdup(path);
    gtk_editable_set_text(GTK_EDITABLE(data->address_bar), path);
}

static void on_address_activate(GtkEntry *entry, gpointer user_data) {
    SFTPViewData *data = (SFTPViewData *)user_data;
    const char *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    update_file_list(data, path);
}

GtkWidget* create_sftp_view() {
    SFTPViewData *data = g_new0(SFTPViewData, 1);
    
    data->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_top(toolbar, 10);
    gtk_widget_set_margin_bottom(toolbar, 10);
    gtk_widget_set_margin_start(toolbar, 10);
    gtk_widget_set_margin_end(toolbar, 10);
    
    data->address_bar = gtk_entry_new();
    gtk_widget_set_hexpand(data->address_bar, TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(data->address_bar), "Not connected");
    gtk_widget_set_sensitive(data->address_bar, FALSE);
    g_signal_connect(data->address_bar, "activate", G_CALLBACK(on_address_activate), data);
    
    gtk_box_append(GTK_BOX(toolbar), gtk_label_new("Path: "));
    gtk_box_append(GTK_BOX(toolbar), data->address_bar);
    
    GtkWidget *btn_go = gtk_button_new_from_icon_name("media-playback-start-symbolic");
    g_signal_connect_swapped(btn_go, "clicked", G_CALLBACK(on_address_activate), data->address_bar);
    gtk_widget_set_sensitive(btn_go, FALSE);
    gtk_box_append(GTK_BOX(toolbar), btn_go);
    
    gtk_box_append(GTK_BOX(data->box), toolbar);
    
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    
    data->list_store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    data->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(data->list_store));
    
    GtkCellRenderer *renderer_icon = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn *col_icon = gtk_tree_view_column_new_with_attributes("", renderer_icon, "icon-name", COL_ICON, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), col_icon);

    GtkCellRenderer *renderer_text = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col_name = gtk_tree_view_column_new_with_attributes("Name", renderer_text, "text", COL_NAME, NULL);
    gtk_tree_view_column_set_expand(col_name, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), col_name);
    
    GtkCellRenderer *renderer_size = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col_size = gtk_tree_view_column_new_with_attributes("Size", renderer_size, "text", COL_SIZE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), col_size);
    
    GtkCellRenderer *renderer_perms = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col_perms = gtk_tree_view_column_new_with_attributes("Permissions", renderer_perms, "text", COL_PERMS, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), col_perms);
    
    g_signal_connect(data->tree_view, "row-activated", G_CALLBACK(on_row_activated), data);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), data->tree_view);
    gtk_box_append(GTK_BOX(data->box), scrolled);
    
    g_object_set_data(G_OBJECT(data->box), "view_data", data);
    g_object_set_data(G_OBJECT(data->box), "btn_go", btn_go); // Save for later
    
    return data->box;
}

void sftp_view_connect(GtkWidget *view, Host *host) {
    SFTPViewData *data = g_object_get_data(G_OBJECT(view), "view_data");
    GtkWidget *btn_go = g_object_get_data(G_OBJECT(view), "btn_go");
    
    if (data->sftp_ctx) {
        sftp_context_free(data->sftp_ctx);
        data->sftp_ctx = NULL;
    }
    if (data->ssh_ctx) {
        ssh_context_free(data->ssh_ctx);
        data->ssh_ctx = NULL;
    }
    
    // Reset UI
    gtk_list_store_clear(data->list_store);
    gtk_editable_set_text(GTK_EDITABLE(data->address_bar), "");
    gtk_widget_set_sensitive(data->address_bar, FALSE);
    if (btn_go) gtk_widget_set_sensitive(btn_go, FALSE);
    
    if (!host) return; // Just disconnect if host is NULL

    data->ssh_ctx = ssh_context_new();
    
    // Note: We use 0 for proxy fields as placeholders
    if (ssh_connect_to_server(data->ssh_ctx, host->hostname, host->port, host->username, host->password, host->key_path, NULL, 0, NULL, NULL, NULL, false) == 0) {
        data->sftp_ctx = sftp_context_new(data->ssh_ctx);
        if (sftp_init_session(data->sftp_ctx) == 0) {
            gtk_widget_set_sensitive(data->address_bar, TRUE);
            if (btn_go) gtk_widget_set_sensitive(btn_go, TRUE);
            update_file_list(data, "."); // Start at home (often .) or get cwd
        } else {
             // Error init SFTP
        }
    } else {
        // Error SSH
    }
}
