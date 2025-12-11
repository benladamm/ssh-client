#include "hosts_view.h"
#include "db.h"
#include "ssh_backend.h"
#include "terminal_view.h"
#include "sftp_view.h"
#include <gtk/gtk.h>

typedef struct {
    GtkWidget *list_box;
    GtkWidget *main_stack;
} HostsViewData;

static void refresh_hosts_list(HostsViewData *data);
static GtkWidget* create_host_row(Host *h, HostsViewData *data);

static void on_connect_clicked(GtkWidget *btn, gpointer user_data) {
    Host *host = (Host*)user_data;
    GtkWidget *main_stack = GTK_WIDGET(g_object_get_data(G_OBJECT(btn), "main_stack"));
    
    if (g_strcmp0(host->protocol, "sftp") == 0) {
        // SFTP
        GtkWidget *sftp_view = gtk_stack_get_child_by_name(GTK_STACK(main_stack), "files");
        sftp_view_connect(sftp_view, host);
        gtk_stack_set_visible_child_name(GTK_STACK(main_stack), "files");
    } else {
        // SSH / Telnet
        GtkWidget *term_view = gtk_stack_get_child_by_name(GTK_STACK(main_stack), "terminal");
        
        Host *proxy = NULL;
        if (host->proxy_host_id > 0) {
            proxy = db_get_host_by_id(host->proxy_host_id);
        }
        
        terminal_view_connect(term_view, host->hostname, host->port, host->username, host->password, host->key_path, host->protocol, proxy);
        
        if (proxy) {
            db_free_host(proxy);
        }
        
        gtk_stack_set_visible_child_name(GTK_STACK(main_stack), "terminal");
    }
}

static void on_save_host(GtkWidget *btn, gpointer user_data) {
    GtkWidget *dialog = GTK_WIDGET(gtk_widget_get_root(btn));
    GtkWidget **entries = (GtkWidget**)user_data;
    HostsViewData *view_data = g_object_get_data(G_OBJECT(dialog), "view_data");
    Host *host_to_edit = g_object_get_data(G_OBJECT(dialog), "host_to_edit");
    
    const char *name = gtk_editable_get_text(GTK_EDITABLE(entries[0]));
    const char *hostname = gtk_editable_get_text(GTK_EDITABLE(entries[1]));
    const char *port_str = gtk_editable_get_text(GTK_EDITABLE(entries[2]));
    const char *user = gtk_editable_get_text(GTK_EDITABLE(entries[3]));
    const char *pass = gtk_editable_get_text(GTK_EDITABLE(entries[4]));
    const char *key = gtk_editable_get_text(GTK_EDITABLE(entries[5]));
    const char *proto = gtk_combo_box_get_active_id(GTK_COMBO_BOX(entries[8]));
    const char *group_id_str = gtk_combo_box_get_active_id(GTK_COMBO_BOX(entries[9]));
    const char *proxy_id_str = gtk_combo_box_get_active_id(GTK_COMBO_BOX(entries[10]));
    
    int port = atoi(port_str);
    int group_id = atoi(group_id_str ? group_id_str : "0");
    int proxy_id = atoi(proxy_id_str ? proxy_id_str : "0");
    
    if (strlen(name) == 0 || strlen(hostname) == 0 || strlen(user) == 0) {
        // TODO: Show error
        return;
    }
    
    if (host_to_edit) {
        db_update_host(host_to_edit->id, name, hostname, port, user, pass, key, proto, group_id, proxy_id);
    } else {
        db_add_host(name, hostname, port, user, pass, key, proto, group_id, proxy_id);
    }
    
    refresh_hosts_list(view_data);
    gtk_window_destroy(GTK_WINDOW(dialog));
    g_free(entries);
}

static void show_host_dialog(GtkWidget *parent, HostsViewData *data, Host *host_to_edit) {
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gtk_widget_get_root(parent)));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_title(GTK_WINDOW(dialog), host_to_edit ? "Edit Host" : "New Host");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 550);
    
    g_object_set_data(G_OBJECT(dialog), "view_data", data);
    g_object_set_data(G_OBJECT(dialog), "host_to_edit", host_to_edit);
    
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
    
    // Password & Key
    gtk_box_append(GTK_BOX(vbox), gtk_label_new(labels[4]));
    entries[4] = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entries[4]), placeholders[4]);
    gtk_entry_set_visibility(GTK_ENTRY(entries[4]), FALSE);
    gtk_box_append(GTK_BOX(vbox), entries[4]);

    gtk_box_append(GTK_BOX(vbox), gtk_label_new(labels[5]));
    entries[5] = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entries[5]), placeholders[5]);
    gtk_box_append(GTK_BOX(vbox), entries[5]);
    
    // Protocol
    gtk_box_append(GTK_BOX(vbox), gtk_label_new("Protocol"));
    GtkWidget *proto_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(proto_combo), "ssh", "SSH");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(proto_combo), "sftp", "SFTP");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(proto_combo), "ftp", "FTP");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(proto_combo), "telnet", "Telnet");
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(proto_combo), "ssh");
    gtk_box_append(GTK_BOX(vbox), proto_combo);
    entries[8] = proto_combo;

    // Group
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

    // Proxy
    gtk_box_append(GTK_BOX(vbox), gtk_label_new("Proxy Jump (Optional)"));
    GtkWidget *proxy_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(proxy_combo), "0", "None");
    
    int host_count = 0;
    Host **hosts = db_get_all_hosts(&host_count);
    for(int i=0; i<host_count; i++) {
        if (host_to_edit && hosts[i]->id == host_to_edit->id) continue; // Don't proxy to self
        char id_str[32];
        sprintf(id_str, "%d", hosts[i]->id);
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(proxy_combo), id_str, hosts[i]->name);
    }
    db_free_hosts(hosts, host_count);
    
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(proxy_combo), "0");
    gtk_box_append(GTK_BOX(vbox), proxy_combo);
    entries[10] = proxy_combo;
    
    // Fill values if editing
    if (host_to_edit) {
        gtk_editable_set_text(GTK_EDITABLE(entries[0]), host_to_edit->name);
        gtk_editable_set_text(GTK_EDITABLE(entries[1]), host_to_edit->hostname);
        char port_buf[16];
        sprintf(port_buf, "%d", host_to_edit->port);
        gtk_editable_set_text(GTK_EDITABLE(entries[2]), port_buf);
        gtk_editable_set_text(GTK_EDITABLE(entries[3]), host_to_edit->username);
        if (host_to_edit->password) gtk_editable_set_text(GTK_EDITABLE(entries[4]), host_to_edit->password);
        if (host_to_edit->key_path) gtk_editable_set_text(GTK_EDITABLE(entries[5]), host_to_edit->key_path);
        if (host_to_edit->protocol) gtk_combo_box_set_active_id(GTK_COMBO_BOX(proto_combo), host_to_edit->protocol);
        
        char group_buf[16];
        sprintf(group_buf, "%d", host_to_edit->group_id);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(group_combo), group_buf);
        
        char proxy_buf[16];
        sprintf(proxy_buf, "%d", host_to_edit->proxy_host_id);
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(proxy_combo), proxy_buf);
    }

    GtkWidget *btn_save = gtk_button_new_with_label("Save");
    gtk_widget_add_css_class(btn_save, "suggested-action");
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_save_host), entries);
    gtk_box_append(GTK_BOX(vbox), btn_save);
    
    gtk_window_present(GTK_WINDOW(dialog));
}

static void on_add_host_clicked(GtkWidget *btn, gpointer user_data) {
    show_host_dialog(btn, (HostsViewData*)user_data, NULL);
}

static void on_edit_host_clicked(GtkWidget *btn, gpointer user_data) {
    int id = GPOINTER_TO_INT(user_data);
    HostsViewData *data = g_object_get_data(G_OBJECT(btn), "view_data");
    Host *h = db_get_host_by_id(id);
    if (h) {
        show_host_dialog(btn, data, h);
    }
}

static void on_delete_host_clicked(GtkWidget *btn, gpointer user_data) {
    int id = GPOINTER_TO_INT(user_data);
    HostsViewData *data = g_object_get_data(G_OBJECT(btn), "view_data");
    db_delete_host(id);
    refresh_hosts_list(data);
}

static void on_delete_group_clicked(GtkWidget *btn, gpointer user_data) {
    int id = GPOINTER_TO_INT(user_data);
    HostsViewData *data = g_object_get_data(G_OBJECT(btn), "view_data");
    db_delete_group(id);
    refresh_hosts_list(data);
}

static void on_save_group(GtkWidget *btn, gpointer user_data) {
    GtkWidget *entry = GTK_WIDGET(user_data);
    GtkWidget *dialog = GTK_WIDGET(gtk_widget_get_root(btn));
    HostsViewData *view_data = g_object_get_data(G_OBJECT(dialog), "view_data");
    
    const char *name = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (strlen(name) > 0) {
        db_add_group(name);
        refresh_hosts_list(view_data);
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_add_group_clicked(GtkWidget *btn, gpointer user_data) {
    HostsViewData *data = (HostsViewData*)user_data;
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gtk_widget_get_root(btn)));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_title(GTK_WINDOW(dialog), "New Group");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 150);
    
    g_object_set_data(G_OBJECT(dialog), "view_data", data);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Group Name");
    gtk_box_append(GTK_BOX(vbox), entry);
    
    GtkWidget *btn_save = gtk_button_new_with_label("Create");
    gtk_widget_add_css_class(btn_save, "suggested-action");
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_save_group), entry);
    gtk_box_append(GTK_BOX(vbox), btn_save);
    
    gtk_window_present(GTK_WINDOW(dialog));
}

static GtkWidget* create_host_row(Host *h, HostsViewData *data) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(row, "host-row");
    
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

// Helper function for refresh_hosts_list
static void add_group_ui(HostsViewData *data, Host **hosts, int host_count, int group_id, const char *group_name, bool is_deletable) {
    GtkWidget *group_row = gtk_list_box_row_new();
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(group_row), FALSE);
    
    GtkWidget *expander = gtk_expander_new(NULL);
    gtk_expander_set_expanded(GTK_EXPANDER(expander), TRUE);
    
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
        gtk_widget_add_css_class(btn_del, "small-button");
        g_object_set_data(G_OBJECT(btn_del), "view_data", data);
        g_signal_connect(btn_del, "clicked", G_CALLBACK(on_delete_group_clicked), GINT_TO_POINTER(group_id));
        gtk_box_append(GTK_BOX(header_box), btn_del);
    }
    
    gtk_expander_set_label_widget(GTK_EXPANDER(expander), header_box);
    
    GtkWidget *group_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(group_content, "group-content");
    
    int count_in_group = 0;
    for (int i = 0; i < host_count; i++) {
        if (hosts[i]->group_id == group_id) {
            gtk_box_append(GTK_BOX(group_content), create_host_row(hosts[i], data));
            count_in_group++;
        }
    }
    
    if (count_in_group == 0) {
        GtkWidget *empty = gtk_label_new("<i>No servers</i>");
        gtk_label_set_use_markup(GTK_LABEL(empty), TRUE);
        gtk_widget_set_margin_top(empty, 8);
        gtk_widget_set_margin_bottom(empty, 8);
        gtk_box_append(GTK_BOX(group_content), empty);
    }
    
    gtk_expander_set_child(GTK_EXPANDER(expander), group_content);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(group_row), expander);
    gtk_list_box_append(GTK_LIST_BOX(data->list_box), group_row);
}

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
    Group **groups = db_get_all_groups(&group_count);
    
    // 1. Named Groups
    for (int g = 0; g < group_count; g++) {
        add_group_ui(data, hosts, host_count, groups[g]->id, groups[g]->name, true);
    }
    
    // 2. "Other" Group (id 0) if there are ungrouped hosts
    int has_ungrouped = 0;
    for (int i = 0; i < host_count; i++) {
        if (hosts[i]->group_id == 0) {
            has_ungrouped = 1;
            break;
        }
    }
    
    if (has_ungrouped || group_count == 0) {
        add_group_ui(data, hosts, host_count, 0, group_count > 0 ? "Others" : "My Servers", false);
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
    GtkWidget *title = gtk_label_new("My Hosts");
    gtk_widget_add_css_class(title, "view-title");
    gtk_box_append(GTK_BOX(header), title);
    
    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(header), spacer);
    
    GtkWidget *btn_add_group = gtk_button_new_with_label("New Group");
    g_signal_connect(btn_add_group, "clicked", G_CALLBACK(on_add_group_clicked), data);
    gtk_box_append(GTK_BOX(header), btn_add_group);

    GtkWidget *btn_add = gtk_button_new_with_label("+ New Server");
    gtk_widget_add_css_class(btn_add, "suggested-action");
    g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_host_clicked), data);
    gtk_box_append(GTK_BOX(header), btn_add);
    
    gtk_box_append(GTK_BOX(box), header);
    
    // List
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
