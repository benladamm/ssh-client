#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <gtk/gtk.h>

typedef enum {
    THEME_DARK = 0,
    THEME_LIGHT = 1
} AppTheme;

void theme_manager_init();
void theme_manager_set_theme(AppTheme theme);
AppTheme theme_manager_get_current_theme();

#endif
