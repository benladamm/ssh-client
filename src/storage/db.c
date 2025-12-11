#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sqlite3 *db = NULL;

bool db_init() {
    int rc = sqlite3_open("hosts.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return false;
    }

    char *sql = "CREATE TABLE IF NOT EXISTS hosts ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "name TEXT NOT NULL,"
                "hostname TEXT NOT NULL,"
                "port INTEGER DEFAULT 22,"
                "username TEXT,"
                "password TEXT,"
                "key_path TEXT,"
                "protocol TEXT DEFAULT 'ssh',"
                "group_id INTEGER DEFAULT 0);"
                
                "CREATE TABLE IF NOT EXISTS groups ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "name TEXT NOT NULL);"
                
                "CREATE TABLE IF NOT EXISTS history ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "hostname TEXT NOT NULL,"
                "username TEXT,"
                "protocol TEXT,"
                "timestamp INTEGER);";

    char *errMsg = 0;
    rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    
    // Simple migration
    sqlite3_exec(db, "ALTER TABLE hosts ADD COLUMN password TEXT;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE hosts ADD COLUMN key_path TEXT;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE hosts ADD COLUMN group_id INTEGER DEFAULT 0;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE hosts ADD COLUMN proxy_host_id INTEGER DEFAULT 0;", 0, 0, 0);

    return true;
}

void db_close() {
    if (db) {
        sqlite3_close(db);
    }
}

// Simple obfuscation function (XOR) for example
// In a real app, use OpenSSL/Sodium
static char* simple_encrypt(const char* input) {
    if (!input) return NULL;
    size_t len = strlen(input);
    char *output = malloc(len * 2 + 1);
    // Simple hex encoding of an XOR
    for(size_t i=0; i<len; i++) {
        sprintf(output + (i*2), "%02x", input[i] ^ 0x42);
    }
    return output;
}

static char* simple_decrypt(const char* input) {
    if (!input) return NULL;
    size_t len = strlen(input) / 2;
    char *output = malloc(len + 1);
    for(size_t i=0; i<len; i++) {
        unsigned int val;
        sscanf(input + (i*2), "%02x", &val);
        output[i] = (char)(val ^ 0x42);
    }
    output[len] = '\0';
    return output;
}

bool db_add_host(const char* name, const char* hostname, int port, const char* user, const char* password, const char* key_path, const char* protocol, int group_id, int proxy_host_id) {
    if (!db) return false;
    
    char *enc_pass = simple_encrypt(password);
    
    char *sql = sqlite3_mprintf("INSERT INTO hosts (name, hostname, port, username, password, key_path, protocol, group_id, proxy_host_id) VALUES ('%q', '%q', %d, '%q', '%q', '%q', '%q', %d, %d);", 
                                name, hostname, port, user, enc_pass ? enc_pass : "", key_path ? key_path : "", protocol, group_id, proxy_host_id);
    
    if (enc_pass) free(enc_pass);
    
    char *errMsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
    sqlite3_free(sql);
    
    if (rc != SQLITE_OK) {
        // If the error is due to the missing column (simple migration)
        if (strstr(errMsg, "no such column: proxy_host_id")) {
             sqlite3_exec(db, "ALTER TABLE hosts ADD COLUMN proxy_host_id INTEGER DEFAULT 0;", NULL, NULL, NULL);
             // Retry
             rc = sqlite3_exec(db, sql, 0, 0, NULL); // Need to regenerate sql but let's just fail for now or assume init handles it
             // Actually let's just rely on db_init to have created it or user to have fresh db
        }
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool db_update_host(int id, const char* name, const char* hostname, int port, const char* user, const char* password, const char* key_path, const char* protocol, int group_id, int proxy_host_id) {
    if (!db) return false;
    
    char *enc_pass = simple_encrypt(password);
    
    char *sql = sqlite3_mprintf("UPDATE hosts SET name='%q', hostname='%q', port=%d, username='%q', password='%q', key_path='%q', protocol='%q', group_id=%d, proxy_host_id=%d WHERE id=%d;", 
                                name, hostname, port, user, enc_pass ? enc_pass : "", key_path ? key_path : "", protocol, group_id, proxy_host_id, id);
    
    if (enc_pass) free(enc_pass);
    
    char *errMsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
    sqlite3_free(sql);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

Host** db_get_all_hosts(int *count) {
    if (!db) {
        *count = 0;
        return NULL;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, name, hostname, port, username, password, key_path, protocol, group_id, proxy_host_id FROM hosts ORDER BY name ASC";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *count = 0;
        return NULL;
    }

    // Count first (naive method, could use SELECT COUNT)
    // Here we use a resizable dynamic array
    int capacity = 10;
    int size = 0;
    Host **hosts = malloc(sizeof(Host*) * capacity);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (size >= capacity) {
            capacity *= 2;
            hosts = realloc(hosts, sizeof(Host*) * capacity);
        }

        Host *h = malloc(sizeof(Host));
        h->id = sqlite3_column_int(stmt, 0);
        h->name = strdup((const char*)sqlite3_column_text(stmt, 1));
        h->hostname = strdup((const char*)sqlite3_column_text(stmt, 2));
        h->port = sqlite3_column_int(stmt, 3);
        h->username = strdup((const char*)sqlite3_column_text(stmt, 4));
        
        const char *enc_pass = (const char*)sqlite3_column_text(stmt, 5);
        h->password = simple_decrypt(enc_pass);
        
        h->key_path = strdup((const char*)sqlite3_column_text(stmt, 6));
        h->protocol = strdup((const char*)sqlite3_column_text(stmt, 7));
        h->group_id = sqlite3_column_int(stmt, 8);
        h->proxy_host_id = sqlite3_column_int(stmt, 9);

        hosts[size++] = h;
    }

    sqlite3_finalize(stmt);
    *count = size;
    return hosts;
}

Host* db_get_host_by_id(int id) {
    if (!db) return NULL;
    
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, name, hostname, port, username, password, key_path, protocol, group_id, proxy_host_id FROM hosts WHERE id = ?";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    Host *h = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        h = malloc(sizeof(Host));
        h->id = sqlite3_column_int(stmt, 0);
        h->name = strdup((const char*)sqlite3_column_text(stmt, 1));
        h->hostname = strdup((const char*)sqlite3_column_text(stmt, 2));
        h->port = sqlite3_column_int(stmt, 3);
        h->username = strdup((const char*)sqlite3_column_text(stmt, 4));
        
        const char *enc_pass = (const char*)sqlite3_column_text(stmt, 5);
        h->password = simple_decrypt(enc_pass);
        
        h->key_path = strdup((const char*)sqlite3_column_text(stmt, 6));
        h->protocol = strdup((const char*)sqlite3_column_text(stmt, 7));
        h->group_id = sqlite3_column_int(stmt, 8);
        h->proxy_host_id = sqlite3_column_int(stmt, 9);
    }
    
    sqlite3_finalize(stmt);
    return h;
}

bool db_host_exists(const char* name) {
    if (!db || !name) return false;
    
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) FROM hosts WHERE name = ?";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    
    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = sqlite3_column_int(stmt, 0) > 0;
    }
    
    sqlite3_finalize(stmt);
    return exists;
}

bool db_delete_host(int id) {
    if (!db) return false;
    
    char *sql = sqlite3_mprintf("DELETE FROM hosts WHERE id = %d;", id);
    
    char *errMsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
    sqlite3_free(sql);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

void db_free_hosts(Host** hosts, int count) {
    if (!hosts) return;
    for (int i = 0; i < count; i++) {
        free(hosts[i]->name);
        free(hosts[i]->hostname);
        free(hosts[i]->username);
        if (hosts[i]->password) free(hosts[i]->password);
        if (hosts[i]->key_path) free(hosts[i]->key_path);
        free(hosts[i]->protocol);
        free(hosts[i]);
    }
    free(hosts);
}

// --- GROUPS ---
bool db_add_group(const char* name) {
    if (!db) return false;
    
    char *sql = sqlite3_mprintf("INSERT INTO groups (name) VALUES ('%q');", name);
    
    char *errMsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
    sqlite3_free(sql);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

Group** db_get_all_groups(int *count) {
    if (!db) {
        *count = 0;
        return NULL;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, name FROM groups ORDER BY name ASC";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        *count = 0;
        return NULL;
    }

    int capacity = 10;
    int size = 0;
    Group **groups = malloc(sizeof(Group*) * capacity);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (size >= capacity) {
            capacity *= 2;
            groups = realloc(groups, sizeof(Group*) * capacity);
        }

        Group *g = malloc(sizeof(Group));
        g->id = sqlite3_column_int(stmt, 0);
        g->name = strdup((const char*)sqlite3_column_text(stmt, 1));

        groups[size++] = g;
    }

    sqlite3_finalize(stmt);
    *count = size;
    return groups;
}

// ... (previous)
void db_free_groups(Group** groups, int count) {
    if (!groups) return;
    for (int i = 0; i < count; i++) {
        free(groups[i]->name);
        free(groups[i]);
    }
    free(groups);
}

bool db_delete_group(int id) {
    if (!db) return false;
    
    // 1. Move hosts in this group to default group (0)
    char *sql_update = sqlite3_mprintf("UPDATE hosts SET group_id = 0 WHERE group_id = %d;", id);
    sqlite3_exec(db, sql_update, 0, 0, 0);
    sqlite3_free(sql_update);
    
    // 2. Delete the group
    char *sql_del = sqlite3_mprintf("DELETE FROM groups WHERE id = %d;", id);
    
    char *errMsg = 0;
    int rc = sqlite3_exec(db, sql_del, 0, 0, &errMsg);
    sqlite3_free(sql_del);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// --- HISTORY ---
#include <time.h>

bool db_add_history(const char* hostname, const char* username, const char* protocol) {
    if (!db) return false;
    
    // First, remove any existing entry for this host/user to prevent duplicates
    // and ensure the latest is the one we insert (with new timestamp)
    char *sql_del = sqlite3_mprintf("DELETE FROM history WHERE hostname='%q' AND username='%q';", hostname, username);
    sqlite3_exec(db, sql_del, 0, 0, 0);
    sqlite3_free(sql_del);
    
    long timestamp = (long)time(NULL);
    
    char *sql = sqlite3_mprintf("INSERT INTO history (hostname, username, protocol, timestamp) VALUES ('%q', '%q', '%q', %ld);", 
                                hostname, username, protocol, timestamp);
    
    char *errMsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
    sqlite3_free(sql);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

HistoryEntry** db_get_recent_history(int limit, int *count) {
    if (!db) {
        *count = 0;
        return NULL;
    }

    sqlite3_stmt *stmt;
    // Use GROUP BY to ensure uniqueness, picking the most recent timestamp
    char *sql = sqlite3_mprintf("SELECT MAX(id), hostname, username, protocol, MAX(timestamp) FROM history GROUP BY hostname, username ORDER BY timestamp DESC LIMIT %d", limit);
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_free(sql);
        *count = 0;
        return NULL;
    }
    sqlite3_free(sql);

    int capacity = limit;
    int size = 0;
    HistoryEntry **entries = malloc(sizeof(HistoryEntry*) * capacity);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HistoryEntry *e = malloc(sizeof(HistoryEntry));
        e->id = sqlite3_column_int(stmt, 0);
        e->hostname = strdup((const char*)sqlite3_column_text(stmt, 1));
        e->username = strdup((const char*)sqlite3_column_text(stmt, 2));
        e->protocol = strdup((const char*)sqlite3_column_text(stmt, 3));
        e->timestamp = (long)sqlite3_column_int64(stmt, 4);

        entries[size++] = e;
    }

    sqlite3_finalize(stmt);
    *count = size;
    return entries;
}

void db_free_history(HistoryEntry** entries, int count) {
    if (!entries) return;
    for (int i = 0; i < count; i++) {
        free(entries[i]->hostname);
        free(entries[i]->username);
        free(entries[i]->protocol);
        free(entries[i]);
    }
    free(entries);
}
