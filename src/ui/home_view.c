#include "home_view.h"
#include "db.h"
#include <time.h>

GtkWidget* create_home_view() {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
    gtk_widget_add_css_class(box, "home-container");
    gtk_widget_set_margin_top(box, 48);
    gtk_widget_set_margin_bottom(box, 48);
    gtk_widget_set_margin_start(box, 48);
    gtk_widget_set_margin_end(box, 48);

    GtkWidget *welcome_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(welcome_box, "welcome-section");
    
    GtkWidget *title = gtk_label_new("Welcome");
    gtk_widget_add_css_class(title, "view-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(welcome_box), title);
    
    GtkWidget *desc = gtk_label_new("Manage your SSH connections with ease");
    gtk_widget_add_css_class(desc, "view-description");
    gtk_widget_set_halign(desc, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(welcome_box), desc);
    
    gtk_box_append(GTK_BOX(box), welcome_box);

    GtkWidget *subtitle = gtk_label_new("Recent Connections");
    gtk_widget_add_css_class(subtitle, "view-subtitle");
    gtk_widget_set_halign(subtitle, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), subtitle);

    GtkWidget *flow = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 4);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 1);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 16);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 16);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
    gtk_box_append(GTK_BOX(box), flow);

    int count = 0;
    HistoryEntry **entries = db_get_recent_history(8, &count);
    
    if (count == 0) {
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
        gtk_widget_add_css_class(empty_box, "empty-state");
        gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(empty_box, GTK_ALIGN_CENTER);
        
        GtkWidget *empty_icon = gtk_image_new_from_icon_name("network-server-symbolic");
        gtk_image_set_pixel_size(GTK_IMAGE(empty_icon), 64);
        gtk_widget_add_css_class(empty_icon, "empty-state-icon");
        gtk_box_append(GTK_BOX(empty_box), empty_icon);
        
        GtkWidget *empty = gtk_label_new("No recent connections");
        gtk_widget_add_css_class(empty, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), empty);
        
        GtkWidget *empty_hint = gtk_label_new("Add a server in the Hosts tab");
        gtk_widget_add_css_class(empty_hint, "dim-label");
        gtk_box_append(GTK_BOX(empty_box), empty_hint);
        
        gtk_box_append(GTK_BOX(box), empty_box);
    } else {
        for (int i = 0; i < count; i++) {
            HistoryEntry *e = entries[i];
            
            GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
            gtk_widget_add_css_class(card, "host-card");
            gtk_widget_set_halign(card, GTK_ALIGN_CENTER);
            
            GtkWidget *icon = gtk_image_new_from_icon_name("network-server-symbolic");
            gtk_image_set_pixel_size(GTK_IMAGE(icon), 40);
            gtk_widget_add_css_class(icon, "card-icon");
            gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
            
            char title_str[256];
            snprintf(title_str, sizeof(title_str), "%s@%s", e->username, e->hostname);
            GtkWidget *lbl = gtk_label_new(title_str);
            gtk_widget_add_css_class(lbl, "card-title");
            gtk_widget_set_halign(lbl, GTK_ALIGN_CENTER);
            gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_MIDDLE);
            gtk_label_set_max_width_chars(GTK_LABEL(lbl), 20);

            GtkWidget *proto = gtk_label_new(e->protocol);
            gtk_widget_add_css_class(proto, "card-subtitle");
            gtk_widget_set_halign(proto, GTK_ALIGN_CENTER);
            
            time_t now = time(NULL);
            double diff = difftime(now, (time_t)e->timestamp);
            char time_str[64];
            if (diff < 60) strcpy(time_str, "Just now");
            else if (diff < 3600) snprintf(time_str, sizeof(time_str), "%d min ago", (int)(diff/60));
            else if (diff < 86400) snprintf(time_str, sizeof(time_str), "%d h ago", (int)(diff/3600));
            else snprintf(time_str, sizeof(time_str), "%d d ago", (int)(diff/86400));
            
            GtkWidget *time_lbl = gtk_label_new(time_str);
            gtk_widget_add_css_class(time_lbl, "dim-label");
            gtk_widget_set_halign(time_lbl, GTK_ALIGN_CENTER);

            gtk_box_append(GTK_BOX(card), icon);
            gtk_box_append(GTK_BOX(card), lbl);
            gtk_box_append(GTK_BOX(card), proto);
            gtk_box_append(GTK_BOX(card), time_lbl);
            
            gtk_flow_box_insert(GTK_FLOW_BOX(flow), card, -1);
        }
        db_free_history(entries, count);
    }

    return box;
}

