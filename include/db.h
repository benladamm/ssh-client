#ifndef DB_H
#define DB_H

#include <sqlite3.h>
#include <stdbool.h>

typedef struct {
    int id;
    char *name;
    char *hostname;
    int port;
    char *username;
    char *password;
    char *key_path;
    char *protocol;
    int group_id;
    int proxy_host_id;
} Host;

typedef struct {
    int id;
    char *name;
} Group;

typedef struct {
    int id;
    char *hostname;
    char *username;
    char *protocol;
    long timestamp;
} HistoryEntry;

// Initialise database
bool db_init();

// Close database connection
void db_close();

// --- HOSTS ---
bool db_add_host(const char* name, const char* hostname, int port, const char* user, const char* password, const char* key_path, const char* protocol, int group_id, int proxy_host_id);

bool db_update_host(int id, const char* name, const char* hostname, int port, const char* user, const char* password, const char* key_path, const char* protocol, int group_id, int proxy_host_id);

Host** db_get_all_hosts(int *count);

Host* db_get_host_by_id(int id);

bool db_host_exists(const char* name);

bool db_delete_host(int id);

void db_free_hosts(Host** hosts, int count);

// --- GROUPS ---
bool db_add_group(const char* name);

bool db_delete_group(int id);

Group** db_get_all_groups(int *count);

void db_free_groups(Group** groups, int count);

// --- HISTORY ---
bool db_add_history(const char* hostname, const char* username, const char* protocol);

HistoryEntry** db_get_recent_history(int limit, int *count);

void db_free_history(HistoryEntry** entries, int count);

#endif
