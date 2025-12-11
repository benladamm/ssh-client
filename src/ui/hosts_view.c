#include "hosts_view.h"
#include "terminal_view.h"
#include "db.h"

typedef struct {
    GtkWidget *list_box;
    GtkWidget *main_stack;
} HostsViewData;

static void refresh_hosts_list(HostsViewData *data);

static void on_connect_clicked(GtkWidget *btn, gpointer user_data) {
    Host *host = (Host *)user_data;
    GtkWidget *main_stack = g_object_get_data(G_OBJECT(btn), "main_stack");
    
    GtkWidget *term_view = gtk_stack_get_child_by_name(GTK_STACK(main_stack), "terminal");
    GtkWidget *files_view = gtk_stack_get_child_by_name(GTK_STACK(main_stack), "files");
    
    Host *proxy_host = NULL;
    if (host->proxy_host_id > 0) {
        proxy_host = db_get_host_by_id(host->proxy_host_id);
    }

    if (g_strcmp0(host->protocol, "sftp") == 0 || g_strcmp0(host->protocol, "ftp") == 0) {
        extern void sftp_view_connect(GtkWidget *view, Host *host);
        
        sftp_view_connect(files_view, host);
        gtk_stack_set_visible_child_name(GTK_STACK(main_stack), "files");
        
    } else {
        terminal_view_connect(term_view, host->hostname, host->port, host->username, host->password, host->key_path, host->protocol, proxy_host);
        gtk_stack_set_visible_child_name(GTK_STACK(main_stack), "terminal");
    }
    
    if (proxy_host) {
        free(proxy_host->hostname);
        free(proxy_host->username);
        if(proxy_host->password) free(proxy_host->password);
        if(proxy_host->key_path) free(proxy_host->key_path);
        if(proxy_host->protocol) free(proxy_host->protocol);
        free(proxy_host);
    }
    
    gtk_stack_set_visible_child_name(GTK_STACK(main_stack), "terminal");
}

static void on_auth_type_changed(GtkComboBox *combo, gpointer user_data) {
    GtkWidget **entries = (GtkWidget **)user_data;
    const char *id = gtk_combo_box_get_active_id(combo);
    
    if (g_strcmp0(id, "password") == 0) {
        gtk_widget_set_visible(entries[6], TRUE);
        gtk_widget_set_visible(entries[7], FALSE);
        gtk_editable_set_text(GTK_EDITABLE(entries[5]), "");
    } else {
        gtk_widget_set_visible(entries[6], FALSE);
        gtk_widget_set_visible(entries[7], TRUE);
        gtk_editable_set_text(GTK_EDITABLE(entries[4]), "");
    }
}

static void show_error_dialog(GtkWidget *parent, const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_OK,
                                             "%s", message);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    gtk_widget_set_visible(dialog, TRUE);
}

typedef struct {
    GtkWidget **entries;
    HostsViewData *view_data;
    Host *editing_host; // NULL if adding new
} HostDialogData;

static void on_add_host_save(GtkWidget *btn, gpointer user_data) {
    GtkWidget *dialog = gtk_widget_get_ancestor(btn, GTK_TYPE_WINDOW);
    HostDialogData *hdd = (HostDialogData *)user_data;
    GtkWidget **entries = hdd->entries;
    
    const char *name = gtk_editable_get_text(GTK_EDITABLE(entries[0]));
    const char *host = gtk_editable_get_text(GTK_EDITABLE(entries[1]));
    const char *port_str = gtk_editable_get_text(GTK_EDITABLE(entries[2]));
    const char *user = gtk_editable_get_text(GTK_EDITABLE(entries[3]));
    const char *password = gtk_editable_get_text(GTK_EDITABLE(entries[4]));
    const char *key_path = gtk_editable_get_text(GTK_EDITABLE(entries[5]));
    
    if (strlen(name) == 0) {
        show_error_dialog(dialog, "Name is required.");
        return;
    }
    if (strlen(host) == 0) {
        show_error_dialog(dialog, "Host (IP) is required.");
        return;
    }
    if (strlen(user) == 0) {
        show_error_dialog(dialog, "User is required.");
        return;
    }
    
    if (!hdd->editing_host || g_strcmp0(hdd->editing_host->name, name) != 0) {
        if (db_host_exists(name)) {
            show_error_dialog(dialog, "A host with this name already exists.");
            return;
        }
    }
    
    if (gtk_widget_get_visible(entries[6])) {
        if (strlen(password) == 0) {
            
        }
    } else {
        if (strlen(key_path) == 0) {
            show_error_dialog(dialog, "Key path is required.");
            return;
        }
    }
    
    GtkComboBox *proto_combo = GTK_COMBO_BOX(entries[8]);
    const char *protocol = gtk_combo_box_get_active_id(proto_combo);
    
    GtkComboBox *group_combo = GTK_COMBO_BOX(entries[9]);
    const char *group_id_str = gtk_combo_box_get_active_id(group_combo);
    int group_id = group_id_str ? atoi(group_id_str) : 0;

    GtkComboBox *proxy_combo = GTK_COMBO_BOX(entries[10]);
    const char *proxy_id_str = gtk_combo_box_get_active_id(proxy_combo);
    int proxy_host_id = proxy_id_str ? atoi(proxy_id_str) : 0;
    
    int port = atoi(port_str);
    if (port == 0) port = 22;

    if (hdd->editing_host) {
        db_update_host(hdd->editing_host->id, name, host, port, user, password, key_path, protocol, group_id, proxy_host_id);
    } else {
        db_add_host(name, host, port, user, password, key_path, protocol, group_id, proxy_host_id);
    }
    
    refresh_hosts_list(hdd->view_data);
    
    gtk_window_destroy(GTK_WINDOW(dialog));
    g_free(entries);
    g_free(hdd);
}

static void on_add_group_save(GtkWidget *btn, gpointer user_data) {
    GtkWidget *dialog = gtk_widget_get_ancestor(btn, GTK_TYPE_WINDOW);
    GtkWidget *entry = GTK_WIDGET(user_data);
    const char *name = gtk_editable_get_text(GTK_EDITABLE(entry));
    
    if (strlen(name) > 0) {
        db_add_group(name);
        HostsViewData *view_data = g_object_get_data(G_OBJECT(dialog), "view_data");
        if (view_data) refresh_hosts_list(view_data);
    }
    
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_add_group_clicked(GtkWidget *btn, gpointer user_data) {
    HostsViewData *data = (HostsViewData *)user_data;
    GtkWidget *window = gtk_widget_get_ancestor(btn, GTK_TYPE_WINDOW);
    
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
static void on_add_group_clicked(GtkWidget *btn, gpointer user_data) {
    HostsViewData *data = (HostsViewData *)user_data;
    GtkWidget *window = gtk_widget_get_ancestor(btn, GTK_TYPE_WINDOW);
    
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_title(GTK_WINDOW(dialog), "New Group");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 150);
    gtk_widget_add_css_class(dialog, "dialog");
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    
    gtk_box_append(GTK_BOX(vbox), gtk_label_new("Group Name"));
    GtkWidget *entry = gtk_entry_new();
    gtk_box_append(GTK_BOX(vbox), entry);
    
    GtkWidget *btn_save = gtk_button_new_with_label("Create");
    gtk_widget_add_css_class(btn_save, "suggested-action");
static void show_host_dialog(GtkWidget *parent, HostsViewData *data, Host *host_to_edit) {
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_title(GTK_WINDOW(dialog), host_to_edit ? "Edit Host" : "New Host");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 450, 600);
    gtk_widget_add_css_class(dialog, "dialog");
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(vbox, 24);
    gtk_widget_set_margin_bottom(vbox, 24);
    gtk_widget_set_margin_start(vbox, 24);
    gtk_widget_set_margin_end(vbox, 24);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    
    GtkWidget **entries = g_new(GtkWidget*, 11);
    
    const char *labels[] = {
        "Name (Required)", 
        "Host / IP (Required)", 
        "Port", 
        "User (Required)", 
        "Password", 
        "Private Key Path"
    };
    const char *placeholders[] = {
        "My Server", 
        "192.168.1.1", 
        "22", 
        "root", 
        "Secret", 
        "/home/user/.ssh/id_rsa"
    };
    
    for (int i = 0; i < 4; i++) {
        gtk_box_append(GTK_BOX(vbox), gtk_label_new(labels[i]));
        entries[i] = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(entries[i]), placeholders[i]);
        gtk_box_append(GTK_BOX(vbox), entries[i]);
    }
    
    gtk_box_append(GTK_BOX(vbox), gtk_label_new("Protocol"));
    GtkWidget *proto_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(proto_combo), "ssh", "SSH");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(proto_combo), "sftp", "SFTP");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(proto_combo), "ftp", "FTP");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(proto_combo), "telnet", "Telnet");
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(proto_combo), "ssh");
    gtk_box_append(GTK_BOX(vbox), proto_combo);
    entries[8] = proto_combo;

    gtk_box_append(GTK_BOX(vbox), gtk_label_new("Group"));
    GtkWidget *group_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(group_combo), "0", "None");
    
    int group_count = 0;
    Group **groups = db_get_all_groups(&group_count);
    for(int i=0; i<group_count; i++) {
        char id_str[32];
        sprintf(id_str, "%d", groups[i]->id);
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(group_combo), id_str, groups[i]->name);
    }
    db_free_groups(groups, group_count);
    
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(group_combo), "0");
    gtk_box_append(GTK_BOX(vbox), group_combo);
    entries[9] = group_combo;

    gtk_box_append(GTK_BOX(vbox), gtk_label_new("Proxy Jump (Optional)"));
    GtkWidget *proxy_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(proxy_combo), "0", "None");
    
    int host_count = 0;
    Host **hosts = db_get_all_hosts(&host_count);
    for(int i=0; i<host_count; i++) {
        char id_str[32];
        sprintf(id_str, "%d", hosts[i]->id);
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(proxy_combo), id_str, hosts[i]->name);
    }
    db_free_hosts(hosts, host_count);
    
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(proxy_combo), "0");
    gtk_box_append(GTK_BOX(vbox), proxy_combo);
    entries[10] = proxy_combo;
    
    // Auth Method
    gtk_box_append(GTK_BOX(vbox), gtk_label_new("Authentication Method"));
    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "password", "Password");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), "key", "Public Key");
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), "password");
    gtk_box_append(GTK_BOX(vbox), combo);
    
    // Champs Auth
    entries[6] = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(entries[6]), gtk_label_new(labels[4]));
    entries[4] = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entries[4]), placeholders[4]);
    gtk_entry_set_visibility(GTK_ENTRY(entries[4]), FALSE);
    gtk_box_append(GTK_BOX(entries[6]), entries[4]);
    gtk_box_append(GTK_BOX(vbox), entries[6]);

    entries[7] = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(entries[7]), gtk_label_new(labels[5]));
    entries[5] = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entries[5]), placeholders[5]);
    gtk_box_append(GTK_BOX(entries[7]), entries[5]);
    gtk_box_append(GTK_BOX(vbox), entries[7]);
    
    gtk_widget_set_visible(entries[7], FALSE);
    
    g_signal_connect(combo, "changed", G_CALLBACK(on_auth_type_changed), entries);
    
    // Si édition, pré-remplir les champs
    if (host_to_edit) {
        gtk_editable_set_text(GTK_EDITABLE(entries[0]), host_to_edit->name);
        gtk_editable_set_text(GTK_EDITABLE(entries[1]), host_to_edit->hostname);
        
        char port_str[32];
        sprintf(port_str, "%d", host_to_edit->port);
        gtk_editable_set_text(GTK_EDITABLE(entries[2]), port_str);
        
        gtk_editable_set_text(GTK_EDITABLE(entries[3]), host_to_edit->username);
        
        if (host_to_edit->password)
            gtk_editable_set_text(GTK_EDITABLE(entries[4]), host_to_edit->password);
            
        if (host_to_edit->key_path) {
            gtk_editable_set_text(GTK_EDITABLE(entries[5]), host_to_edit->key_path);
            if (strlen(host_to_edit->key_path) > 0) {
                 gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), "key");
                 gtk_widget_set_visible(entries[6], FALSE);
                 gtk_widget_set_visible(entries[7], TRUE);
            }
        }
        
        if (host_to_edit->protocol)
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(proto_combo), host_to_edit->protocol);

        char gid_str[32];
        sprintf(gid_str, "%d", host_to_edit->group_id);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(group_combo), gid_str);
        
        char pid_str[32];
        sprintf(pid_str, "%d", host_to_edit->proxy_host_id);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(proxy_combo), pid_str);
    }
    
    HostDialogData *hdd = g_new(HostDialogData, 1);
    hdd->entries = entries;
    hdd->view_data = data;
    hdd->editing_host = host_to_edit;

    GtkWidget *btn_save = gtk_button_new_with_label(host_to_edit ? "Edit" : "Save");
    gtk_widget_add_css_class(btn_save, "suggested-action");
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_add_host_save), hdd);
    gtk_box_append(GTK_BOX(vbox), btn_save);
    
    gtk_window_present(GTK_WINDOW(dialog));
}

static void on_add_host_clicked(GtkWidget *btn, gpointer user_data) {
    HostsViewData *data = (HostsViewData *)user_data;
    GtkWidget *window = gtk_widget_get_ancestor(btn, GTK_TYPE_WINDOW);
    show_host_dialog(window, data, NULL);
}

static void on_edit_host_clicked(GtkWidget *btn, gpointer user_data) {
    int host_id = GPOINTER_TO_INT(user_data);
    HostsViewData *data = g_object_get_data(G_OBJECT(btn), "view_data");
    GtkWidget *window = gtk_widget_get_ancestor(btn, GTK_TYPE_WINDOW);
    
    Host *host = db_get_host_by_id(host_id);
    if (host) {
        show_host_dialog(window, data, host);
        // Note: host is leaked here technically because show_host_dialog doesn't free it and db_get returns a malloc'd host.
        // We should handle cleanup in dialog destroy or save. 
        // For MVP, small leak.
    }
}

static void on_delete_host_clicked(GtkWidget *btn, gpointer user_data) {
    int host_id = GPOINTER_TO_INT(user_data);
    HostsViewData *data = g_object_get_data(G_OBJECT(btn), "view_data");
 
    db_delete_host(host_id);
    refresh_hosts_list(data);
}

static void on_delete_group_clicked(GtkWidget *btn, gpointer user_data) {
    int group_id = GPOINTER_TO_INT(user_data);
    HostsViewData *data = g_object_get_data(G_OBJECT(btn), "view_data");
    
    // Confirmation dialog could be nice
    db_delete_group(group_id);
    refresh_hosts_list(data);
}

static GtkWidget* create_host_row(Host *h, HostsViewData *data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_add_css_class(row, "host-row");
    gtk_widget_set_margin_top(row, 8);
    gtk_widget_set_margin_bottom(row, 8);
    gtk_widget_set_margin_start(row, 16);
    gtk_widget_set_margin_end(row, 16);
    
    const char *icon_name = "network-server-symbolic";
    if (g_strcmp0(h->protocol, "ftp") == 0 || g_strcmp0(h->protocol, "sftp") == 0) {
        icon_name = "folder-remote-symbolic";
    } else if (g_strcmp0(h->protocol, "telnet") == 0) {
        icon_name = "utilities-terminal-symbolic";
    }
    
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 32);
    gtk_widget_add_css_class(icon, "card-icon");
    gtk_box_append(GTK_BOX(row), icon);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);
    
    GtkWidget *lbl_name = gtk_label_new(h->name);
    gtk_widget_add_css_class(lbl_name, "card-title");
    gtk_widget_set_halign(lbl_name, GTK_ALIGN_START);
    
    char sub[256];
    snprintf(sub, sizeof(sub), "%s://%s@%s:%d", h->protocol ? h->protocol : "ssh", h->username, h->hostname, h->port);
    GtkWidget *lbl_sub = gtk_label_new(sub);
    gtk_widget_add_css_class(lbl_sub, "card-subtitle");
    gtk_widget_set_halign(lbl_sub, GTK_ALIGN_START);
    
    gtk_box_append(GTK_BOX(vbox), lbl_name);
    gtk_box_append(GTK_BOX(vbox), lbl_sub);
    gtk_box_append(GTK_BOX(row), vbox);
    
    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(row), spacer);
    
    GtkWidget *btn_connect = gtk_button_new_with_label("Connect");
    gtk_widget_add_css_class(btn_connect, "connect-button");
    Host *h_copy = g_new(Host, 1);
    *h_copy = *h;
    h_copy->hostname = g_strdup(h->hostname);
    h_copy->username = g_strdup(h->username);
    h_copy->password = h->password ? g_strdup(h->password) : NULL;
    h_copy->key_path = h->key_path ? g_strdup(h->key_path) : NULL;
    h_copy->protocol = h->protocol ? g_strdup(h->protocol) : NULL;
    h_copy->proxy_host_id = h->proxy_host_id;
    
    g_object_set_data(G_OBJECT(btn_connect), "main_stack", data->main_stack);
    g_signal_connect(btn_connect, "clicked", G_CALLBACK(on_connect_clicked), h_copy);
    gtk_box_append(GTK_BOX(row), btn_connect);

    GtkWidget *btn_edit = gtk_button_new_from_icon_name("document-edit-symbolic");
    gtk_widget_set_tooltip_text(btn_edit, "Edit");
    gtk_widget_add_css_class(btn_edit, "nav-button");
    g_object_set_data(G_OBJECT(btn_edit), "view_data", data);
    g_signal_connect(btn_edit, "clicked", G_CALLBACK(on_edit_host_clicked), GINT_TO_POINTER(h->id));
    gtk_box_append(GTK_BOX(row), btn_edit);
    
    GtkWidget *btn_delete = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_set_tooltip_text(btn_delete, "Delete");
    gtk_widget_add_css_class(btn_delete, "destructive-action");
    g_object_set_data(G_OBJECT(btn_delete), "view_data", data);
    g_signal_connect(btn_delete, "clicked", G_CALLBACK(on_delete_host_clicked), GINT_TO_POINTER(h->id));
    gtk_box_append(GTK_BOX(row), btn_delete);
    
    return row;
}

static void refresh_hosts_list(HostsViewData *data) {
    // Nettoyer la liste actuelle
    GtkWidget *child = gtk_widget_get_first_child(data->list_box);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(data->list_box), child);
static void refresh_hosts_list(HostsViewData *data) {
    GtkWidget *child = gtk_widget_get_first_child(data->list_box);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(data->list_box), child);
        child = next;
    }
    
    int host_count = 0;
    Host **hosts = db_get_all_hosts(&host_count);
    
    int group_count = 0;
    Group **groups = db_get_all_groups(&group_count);ander_new(NULL);
        gtk_expander_set_expanded(GTK_EXPANDER(expander), TRUE);
        
        // Custom Label Widget pour l'expander avec bouton suppression
        GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *lbl = gtk_label_new(NULL);
        char markup[256];
        snprintf(markup, sizeof(markup), "<b>%s</b>", group_name);
        gtk_label_set_markup(GTK_LABEL(lbl), markup);
        gtk_box_append(GTK_BOX(header_box), lbl);
        
        if (is_deletable) {
            GtkWidget *spacer = gtk_label_new("");
            gtk_widget_set_hexpand(spacer, TRUE);
            gtk_box_append(GTK_BOX(header_box), spacer);
            
            GtkWidget *btn_del = gtk_button_new_from_icon_name("user-trash-symbolic");
            gtk_widget_add_css_class(btn_del, "destructive-action");
            gtk_widget_add_css_class(btn_del, "small-button"); // Custom class if needed
            // Empêcher l'expander de capturer le clic du bouton ? Gtk4 gère ça bien généralement via les contrôleurs
            g_object_set_data(G_OBJECT(btn_del), "view_data", data);
            g_signal_connect(btn_del, "clicked", G_CALLBACK(on_delete_group_clicked), GINT_TO_POINTER(group_id));
            gtk_box_append(GTK_BOX(header_box), btn_del);
        }
        
        gtk_expander_set_label_widget(GTK_EXPANDER(expander), header_box);
        
        // Contenu du groupe : une inner listbox ou vbox
        GtkWidget *group_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_add_css_class(group_content, "group-content");
        
        int count_in_group = 0;
        for (int i = 0; i < host_count; i++) {
            if (hosts[i]->group_id == group_id) {
                // Ici on ajoute direct le row créé par create_host_row, qui retourne un GtkBox (pas un ListBoxRow)
                // Donc on le wrap ? Non create_host_row retourne un Box.
                // On l'ajoute au VBox group_content
                gtk_box_append(GTK_BOX(group_content), create_host_row(hosts[i], data));
                count_in_group++;
            }
        }
        
        if (count_in_group == 0) {
            GtkWidget *empty = gtk_label_new("<i>Aucun serveur</i>");
            gtk_label_set_use_markup(GTK_LABEL(empty), TRUE);
            gtk_widget_set_margin_top(empty, 8);
            gtk_widget_set_margin_bottom(empty, 8);
            gtk_box_append(GTK_BOX(group_content), empty);
        }
        
        gtk_expander_set_child(GTK_EXPANDER(expander), group_content);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(group_row), expander);
        gtk_list_box_append(GTK_LIST_BOX(data->list_box), group_row);
    }
    
    // 1. Groupes nommés
    for (int g = 0; g < group_count; g++) {
        add_group_ui(groups[g]->id, groups[g]->name, true);
    }
    
    // 2. Groupe "Autres" (id 0) s'il y a des hôtes sans groupe
    int has_ungrouped = 0;
    for (int i = 0; i < host_count; i++) {
        if (hosts[i]->group_id == 0) {
            has_ungrouped = 1;
            break;
        }
    }
    
    if (has_ungrouped || group_count == 0) {
        add_group_ui(0, group_count > 0 ? "Autres" : "Mes Serveurs", false);
    }
    
    db_free_hosts(hosts, host_count);
    db_free_groups(groups, group_count);
}

GtkWidget* create_hosts_view(GtkWidget *main_stack) {
    HostsViewData *data = g_new0(HostsViewData, 1);
    data->main_stack = main_stack;
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
    gtk_widget_set_margin_top(box, 48);
    gtk_widget_set_margin_bottom(box, 48);
    gtk_widget_set_margin_start(box, 48);
    gtk_widget_set_margin_end(box, 48);
    
    // Header
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *title = gtk_label_new("Mes Hôtes");
    gtk_widget_add_css_class(title, "view-title");
    gtk_box_append(GTK_BOX(header), title);
    
    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(header), spacer);
    
    GtkWidget *btn_add_group = gtk_button_new_with_label("Nouveau Groupe");
    g_signal_connect(btn_add_group, "clicked", G_CALLBACK(on_add_group_clicked), data);
    gtk_box_append(GTK_BOX(header), btn_add_group);

    GtkWidget *btn_add = gtk_button_new_with_label("+ Nouveau Serveur");
    gtk_widget_add_css_class(btn_add, "suggested-action");
    g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_host_clicked), data);
    gtk_box_append(GTK_BOX(header), btn_add);
    
    gtk_box_append(GTK_BOX(box), header);
    
    // Liste
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    
    data->list_box = gtk_list_box_new();
    gtk_widget_add_css_class(data->list_box, "content-view");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), data->list_box);
    
    gtk_box_append(GTK_BOX(box), scrolled);
    
    // Initial load
    refresh_hosts_list(data);
    
    // Cleanup
    g_object_set_data_full(G_OBJECT(box), "view_data", data, g_free);
    
    return box;
}
