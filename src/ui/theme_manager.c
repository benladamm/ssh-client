#include "theme_manager.h"

static AppTheme current_theme = THEME_DARK;
static GtkCssProvider *current_provider = NULL;

void theme_manager_init() {
    theme_manager_set_theme(THEME_DARK);
}

void theme_manager_set_theme(AppTheme theme) {
    GdkDisplay *display = gdk_display_get_default();
    if (!display) return;

    if (current_provider) {
        gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(current_provider));
        g_object_unref(current_provider);
        current_provider = NULL;
    }

    current_provider = gtk_css_provider_new();
    const char *css_file = (theme == THEME_LIGHT) ? "style-light.css" : "style.css";
    
    gtk_css_provider_load_from_path(current_provider, css_file);
    
    gtk_style_context_add_provider_for_display(display,
                                               GTK_STYLE_PROVIDER(current_provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    current_theme = theme;
}

AppTheme theme_manager_get_current_theme() {
    return current_theme;
}
