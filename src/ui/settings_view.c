#include "settings_view.h"
#include "theme_manager.h"

static void on_theme_changed(GtkComboBox *combo, gpointer user_data) {
    int active = gtk_combo_box_get_active(combo);
    if (active == 0) {
        theme_manager_set_theme(THEME_DARK);
    } else {
        theme_manager_set_theme(THEME_LIGHT);
    }
}

GtkWidget* create_settings_view() {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_top(box, 30);
    gtk_widget_set_margin_bottom(box, 30);
    gtk_widget_set_margin_start(box, 30);
    gtk_widget_set_margin_end(box, 30);

    GtkWidget *title = gtk_label_new("Settings");
    gtk_widget_add_css_class(title, "view-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_box_append(GTK_BOX(box), content_box);

    GtkWidget *theme_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *theme_label = gtk_label_new("Theme:");
    GtkWidget *theme_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(theme_combo), "Dark (Default)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(theme_combo), "Light");
    
    if (theme_manager_get_current_theme() == THEME_DARK) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(theme_combo), 0);
    } else {
        gtk_combo_box_set_active(GTK_COMBO_BOX(theme_combo), 1);
    }
    
    g_signal_connect(theme_combo, "changed", G_CALLBACK(on_theme_changed), NULL);
    
    gtk_box_append(GTK_BOX(theme_row), theme_label);
    gtk_box_append(GTK_BOX(theme_row), theme_combo);
    gtk_box_append(GTK_BOX(content_box), theme_row);

    GtkWidget *about_frame = gtk_frame_new("About");
    GtkWidget *about_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(about_box, 10);
    gtk_widget_set_margin_bottom(about_box, 10);
    gtk_widget_set_margin_start(about_box, 10);
    gtk_widget_set_margin_end(about_box, 10);
    
    gtk_frame_set_child(GTK_FRAME(about_frame), about_box);
    
    GtkWidget *app_name = gtk_label_new("Modern SSH Client");
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    pango_attr_list_insert(attrs, pango_attr_scale_new(1.2));
    gtk_label_set_attributes(GTK_LABEL(app_name), attrs);
    pango_attr_list_unref(attrs);

    GtkWidget *version = gtk_label_new("Version 1.0.0");
    GtkWidget *author = gtk_label_new("Developed by Benladamm");
    
    gtk_box_append(GTK_BOX(about_box), app_name);
    gtk_box_append(GTK_BOX(about_box), version);
    gtk_box_append(GTK_BOX(about_box), author);
    
    gtk_box_append(GTK_BOX(content_box), about_frame);

    return box;
}
