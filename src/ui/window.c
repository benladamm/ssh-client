#include "window.h"
#include "home_view.h"
#include "hosts_view.h"
#include "terminal_view.h"
#include "sftp_view.h"
#include "settings_view.h"

static GtkWidget *home_btn;
static GtkWidget *hosts_btn;
static GtkWidget *term_btn;
static GtkWidget *files_btn;
static GtkWidget *settings_btn;

static GtkWidget *current_active_button = NULL;

static void update_button_active_state(GtkWidget *new_active) {
    if (current_active_button) {
        gtk_widget_remove_css_class(current_active_button, "nav-button-active");
    }
    if (new_active) {
        gtk_widget_add_css_class(new_active, "nav-button-active");
        current_active_button = new_active;
    }
}

// Redefine handler to be safer
static void on_nav_button_clicked(GtkWidget *btn, gpointer user_data) {
    
    if (btn == home_btn) {
        gtk_stack_set_visible_child_name(GTK_STACK(stack), "home");
    } else if (btn == hosts_btn) {
        gtk_stack_set_visible_child_name(GTK_STACK(stack), "hosts");
    } else if (btn == files_btn) {
        gtk_stack_set_visible_child_name(GTK_STACK(stack), "files");
    } else if (btn == term_btn) {
        gtk_stack_set_visible_child_name(GTK_STACK(stack), "terminal");
    } else if (btn == settings_btn) {
        gtk_stack_set_visible_child_name(GTK_STACK(stack), "settings");
    }
    
    update_button_active_state(btn);
}

static void on_fullscreen_clicked(GtkWidget *btn, gpointer user_data) {
    GtkRoot *root = gtk_widget_get_root(btn);
static void on_fullscreen_clicked(GtkWidget *btn, gpointer user_data) {
    GtkRoot *root = gtk_widget_get_root(btn);
    if (GTK_IS_WINDOW(root)) {
        GtkWindow *window = GTK_WINDOW(root);
        
        gboolean is_fullscreen = gtk_window_is_fullscreen(window);
        gboolean is_maximized = gtk_window_is_maximized(window);
        
        if (is_fullscreen) {
            gtk_window_unfullscreen(window);
        } else {
            gtk_window_fullscreen(window);
        }
    }
}tkWidget* create_main_window(GtkApplication *app) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Modern SSH Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), main_box);

    // Sidebar
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(sidebar, "sidebar");
    gtk_widget_set_size_request(sidebar, 240, -1);
    gtk_box_append(GTK_BOX(main_box), sidebar);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), main_box);

    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(sidebar, "sidebar");
    gtk_widget_set_size_request(sidebar, 240, -1);
    gtk_box_append(GTK_BOX(main_box), sidebar);
    
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(header_box, "sidebar-header");
    gtk_widget_set_margin_bottom(header_box, 20);
    
    GtkWidget *logo_icon = gtk_image_new_from_icon_name("utilities-terminal-symbolic");
    gtk_widget_add_css_class(logo_icon, "sidebar-logo");
    gtk_image_set_pixel_size(GTK_IMAGE(logo_icon), 32);
    
    GtkWidget *app_title = gtk_label_new("SSH Client");
    gtk_widget_add_css_class(app_title, "sidebar-title");
    
    gtk_box_append(GTK_BOX(header_box), logo_icon);
    gtk_box_append(GTK_BOX(header_box), app_title);
    gtk_box_append(GTK_BOX(sidebar), header_box);

    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 200);
    gtk_widget_set_hexpand(stack, TRUE);
    gtk_widget_set_vexpand(stack, TRUE);
    gtk_box_append(GTK_BOX(main_box), stack);

    GtkWidget* create_nav_btn(const char* label, const char* icon_name) {
        GtkWidget *btn = gtk_button_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
        GtkWidget *lbl = gtk_label_new(label);
        gtk_box_append(GTK_BOX(box), icon);
        gtk_box_append(GTK_BOX(box), lbl);
        gtk_button_set_child(GTK_BUTTON(btn), box);
        gtk_widget_set_halign(box, GTK_ALIGN_START);
        
        gtk_widget_add_css_class(btn, "nav-button");
        gtk_button_set_has_frame(GTK_BUTTON(btn), FALSE);
        
        return btn;
    }tk_box_append(GTK_BOX(sidebar), hosts_btn);
    gtk_box_append(GTK_BOX(sidebar), files_btn);
// ... (previous code)



// ... (existing code for create_main_window)

    gtk_box_append(GTK_BOX(sidebar), term_btn);
    
    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(sidebar), spacer);
    
    // Fullscreen Button
    GtkWidget *fullscreen_btn = create_nav_btn("Fullscreen", "view-fullscreen-symbolic");
    g_signal_connect(fullscreen_btn, "clicked", G_CALLBACK(on_fullscreen_clicked), window);
    gtk_box_append(GTK_BOX(sidebar), fullscreen_btn);
    
    gtk_box_append(GTK_BOX(sidebar), settings_btn);
    
    // ... (rest of function)

    // Views
    GtkWidget *home_view = create_home_view();
    GtkWidget *hosts_view = create_hosts_view(stack);
    GtkWidget *term_view = create_terminal_view();
    GtkWidget *files_view = create_sftp_view();
    GtkWidget *settings_view = create_settings_view();

    gtk_stack_add_named(GTK_STACK(stack), home_view, "home");
    gtk_stack_add_named(GTK_STACK(stack), hosts_view, "hosts");
    gtk_stack_add_named(GTK_STACK(stack), term_view, "terminal");
    gtk_stack_add_named(GTK_STACK(stack), files_view, "files");
    gtk_stack_add_named(GTK_STACK(stack), settings_view, "settings");

    // Init active state
    update_button_active_state(home_btn);
    GtkWidget *home_view = create_home_view();
    GtkWidget *hosts_view = create_hosts_view(stack);
    GtkWidget *term_view = create_terminal_view();
    GtkWidget *files_view = create_sftp_view();
    GtkWidget *settings_view = create_settings_view();

    gtk_stack_add_named(GTK_STACK(stack), home_view, "home");
    gtk_stack_add_named(GTK_STACK(stack), hosts_view, "hosts");
    gtk_stack_add_named(GTK_STACK(stack), term_view, "terminal");
    gtk_stack_add_named(GTK_STACK(stack), files_view, "files");
    gtk_stack_add_named(GTK_STACK(stack), settings_view, "settings");

    update_button_active_state(home_btn);
    
    g_signal_connect(home_btn, "clicked", G_CALLBACK(on_nav_button_clicked), stack);
    g_signal_connect(hosts_btn, "clicked", G_CALLBACK(on_nav_button_clicked), stack);
    g_signal_connect(files_btn, "clicked", G_CALLBACK(on_nav_button_clicked), stack);
    g_signal_connect(term_btn, "clicked", G_CALLBACK(on_nav_button_clicked), stack);
    g_signal_connect(settings_btn, "clicked", G_CALLBACK(on_nav_button_clicked), stack);