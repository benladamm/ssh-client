#ifndef SSH_BACKEND_H
#define SSH_BACKEND_H

#include <libssh/libssh.h>
#include <stdbool.h>

typedef struct {
    ssh_session session;
    ssh_channel channel;
    bool is_connected;
    
    // Proxy fields
    ssh_session proxy_session;
    ssh_channel proxy_channel;
    int proxy_fd;
} SSHContext;

SSHContext* ssh_context_new();

void ssh_context_free(SSHContext* ctx);

int ssh_connect_to_server(SSHContext* ctx, const char* hostname, int port, const char* user, const char* password, const char* key_path,
                          const char* proxy_hostname, int proxy_port, const char* proxy_user, const char* proxy_password, const char* proxy_key_path,
                          bool open_shell_flag);

void ssh_proxy_poll(SSHContext* ctx);

int ssh_open_shell(SSHContext* ctx);

int ssh_read_nonblocking(SSHContext* ctx, char* buffer, size_t max_len);

int ssh_write_data(SSHContext* ctx, const char* buffer, size_t len);

bool ssh_is_channel_open(SSHContext* ctx);

const char* ssh_get_error_msg(SSHContext* ctx);

#endif
