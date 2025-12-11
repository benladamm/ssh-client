cmake_minimum_required(VERSION 3.17)
project(ModernSSH C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wno-deprecated-declarations)

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(LIBSSH REQUIRED libssh)
pkg_check_modules(VTE REQUIRED vte-2.91-gtk4)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

find_package(Threads REQUIRED)

# Include headers
include_directories(include)
include_directories(${GTK4_INCLUDE_DIRS})
include_directories(${LIBSSH_INCLUDE_DIRS})
include_directories(${VTE_INCLUDE_DIRS})
include_directories(${SQLITE3_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.c
    src/ssh_backend.c
    src/storage/db.c
    src/ui/window.c
    src/ui/home_view.c
    src/ui/hosts_view.c
    src/ui/terminal_view.c
    src/ssh_sftp.c
    src/ui/sftp_view.c
    src/ui/settings_view.c
    src/ui/theme_manager.c
)

# Executable
add_executable(modern_ssh ${SOURCES})

# Link libraries
# Explicitly link with libraries found by PkgConfig
target_link_libraries(modern_ssh ${GTK4_LIBRARIES} ${LIBSSH_LIBRARIES} ${VTE_LIBRARIES} ${SQLITE3_LIBRARIES} Threads::Threads)

# Copie du fichier CSS dans le dossier de build pour l'exécution locale
configure_file(resources/style.css ${CMAKE_BINARY_DIR}/style.css COPYONLY)
configure_file(resources/style-light.css ${CMAKE_BINARY_DIR}/style-light.css COPYONLY)

# Installation (Requis pour Flatpak/Packaging)
include(GNUInstallDirs)

# Définir le chemin des ressources pour le code C
add_compile_definitions(RESOURCE_DIR="${CMAKE_INSTALL_FULL_DATADIR}/modern_ssh")

install(TARGETS modern_ssh
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Installation des ressources (CSS) dans /share/modern_ssh
install(FILES 
    resources/style.css 
    resources/style-light.css
    DESTINATION ${CMAKE_INSTALL_DATADIR}/modern_ssh
)

# Installation du fichier Desktop
install(FILES io.github.benladamm.ssh_client.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# Installation du fichier Metainfo (AppStream)
install(FILES io.github.benladamm.ssh_client.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

# Installation de l'icône
install(FILES resources/icon.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    RENAME io.github.benladamm.ssh_client.svg
)
