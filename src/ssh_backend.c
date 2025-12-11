#include "ssh_backend.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

void ssh_proxy_poll(SSHContext* ctx);
int ssh_open_shell(SSHContext* ctx);

SSHContext* ssh_context_new() {
    SSHContext* ctx = malloc(sizeof(SSHContext));
    ctx->session = ssh_new();
    ctx->channel = NULL;
    ctx->is_connected = false;
    ctx->proxy_session = NULL;
    ctx->proxy_channel = NULL;
    ctx->proxy_fd = -1;
    if (ctx->session == NULL) {
        free(ctx);
        return NULL;
    }
    return ctx;
}

void ssh_context_free(SSHContext* ctx) {
    if (ctx) {
        if (ctx->channel) {
            ssh_channel_send_eof(ctx->channel);
            ssh_channel_close(ctx->channel);
            ssh_channel_free(ctx->channel);
        }
        if (ctx->is_connected) {
            ssh_disconnect(ctx->session);
        }
        ssh_free(ctx->session);
        
        if (ctx->proxy_fd != -1) {
            close(ctx->proxy_fd);
        }
        if (ctx->proxy_channel) {
            ssh_channel_close(ctx->proxy_channel);
            ssh_channel_free(ctx->proxy_channel);
        }
        if (ctx->proxy_session) {
            ssh_disconnect(ctx->proxy_session);
            ssh_free(ctx->proxy_session);
        }
        
        free(ctx);
    }
}

static int authenticate_session(ssh_session session, const char* password) {
    int rc = ssh_userauth_publickey_auto(session, NULL, NULL);
    if (rc == SSH_AUTH_SUCCESS) return 0;

    if (password && password[0] != '\0') {
        rc = ssh_userauth_password(session, NULL, password);
        if (rc == SSH_AUTH_SUCCESS) return 0;
    }
    return -1;
}

typedef struct {
    SSHContext* ctx;
    volatile bool running;
} PumpArgs;

static void* proxy_pump_thread_func(void* arg) {
    PumpArgs* args = (PumpArgs*)arg;
    while (args->running) {
        ssh_proxy_poll(args->ctx);
        usleep(1000);
    }
    return NULL;
}

int ssh_connect_to_server(SSHContext* ctx, const char* hostname, int port, const char* user, const char* password, const char* key_path,
                          const char* proxy_hostname, int proxy_port, const char* proxy_user, const char* proxy_password, const char* proxy_key_path,
                          bool open_shell_flag) {
    if (!ctx || !ctx->session) return -1;

    ssh_options_set(ctx->session, SSH_OPTIONS_HOST, hostname);
    ssh_options_set(ctx->session, SSH_OPTIONS_PORT, &port);
    
    printf("Connecting to %s:%d\n", hostname, port);

    if (proxy_hostname && proxy_hostname[0] != '\0') {
        printf("Using proxy %s:%d\n", proxy_hostname, proxy_port);
        ctx->proxy_session = ssh_new();
        if (!ctx->proxy_session) {
            printf("Failed to create proxy session\n");
            return -1;
        }
        
        ssh_options_set(ctx->proxy_session, SSH_OPTIONS_HOST, proxy_hostname);
        ssh_options_set(ctx->proxy_session, SSH_OPTIONS_PORT, &proxy_port);
        ssh_options_set(ctx->proxy_session, SSH_OPTIONS_USER, proxy_user);
        if (proxy_key_path && proxy_key_path[0] != '\0') {
            ssh_options_set(ctx->proxy_session, SSH_OPTIONS_IDENTITY, proxy_key_path);
        }
        
        if (ssh_connect(ctx->proxy_session) != SSH_OK) {
            printf("Failed to connect to proxy: %s\n", ssh_get_error(ctx->proxy_session));
            return -1;
        }
        if (authenticate_session(ctx->proxy_session, proxy_password) != 0) {
            printf("Failed to authenticate to proxy\n");
            return -1;
        }
        
        printf("Proxy connected and authenticated\n");

        ctx->proxy_channel = ssh_channel_new(ctx->proxy_session);
        if (!ctx->proxy_channel) return -1;
        
        if (ssh_channel_open_forward(ctx->proxy_channel, hostname, port, "localhost", 0) != SSH_OK) {
            printf("Failed to open forward channel: %s\n", ssh_get_error(ctx->proxy_session));
            return -1;
        }
        
        printf("Forward channel opened\n");

        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
            perror("socketpair");
            return -1;
        }
        
        fcntl(sv[0], F_SETFL, O_NONBLOCK);
        fcntl(sv[1], F_SETFL, O_NONBLOCK);
        
        ctx->proxy_fd = sv[0];
        
        ssh_options_set(ctx->session, SSH_OPTIONS_FD, &sv[1]);
    }

    ssh_options_set(ctx->session, SSH_OPTIONS_USER, user);

    if (key_path && key_path[0] != '\0') {
        ssh_options_set(ctx->session, SSH_OPTIONS_IDENTITY, key_path);
    }

    pthread_t thread;
    PumpArgs args = {ctx, true};
    if (ctx->proxy_fd != -1) {
        printf("Starting proxy pump thread\n");
        pthread_create(&thread, NULL, proxy_pump_thread_func, &args);
    }

    printf("Connecting to target session...\n");
    int rc = ssh_connect(ctx->session);
    printf("ssh_connect returned %d\n", rc);
    
    if (rc == SSH_OK) {
        if (authenticate_session(ctx->session, password) == 0) {
            ctx->is_connected = true;
            rc = 0;
            
            if (open_shell_flag) {
                if (ssh_open_shell(ctx) != 0) {
                    printf("Failed to open shell\n");
                    rc = -1;
                    ctx->is_connected = false;
                }
            }
        } else {
            printf("Authentication failed\n");
            rc = -1;
        }
    } else {
        printf("Connection failed: %s\n", ssh_get_error(ctx->session));
        rc = -1;
    }

    if (ctx->proxy_fd != -1) {
        printf("Stopping proxy pump thread\n");
        args.running = false;
        pthread_join(thread, NULL);
    }

    return rc;
}

void ssh_proxy_poll(SSHContext* ctx) {
    if (!ctx || !ctx->proxy_channel || ctx->proxy_fd == -1) return;
    
    char buffer[4096];
    int nbytes;
    
    if (ssh_channel_poll(ctx->proxy_channel, 0) > 0) {
        nbytes = ssh_channel_read_nonblocking(ctx->proxy_channel, buffer, sizeof(buffer), 0);
        if (nbytes > 0) {
            write(ctx->proxy_fd, buffer, nbytes);
        }
    }
    
    nbytes = read(ctx->proxy_fd, buffer, sizeof(buffer));
    if (nbytes > 0) {
        ssh_channel_write(ctx->proxy_channel, buffer, nbytes);
    }
}

int ssh_open_shell(SSHContext* ctx) {
    if (!ctx || !ctx->session || !ctx->is_connected) return -1;

    ctx->channel = ssh_channel_new(ctx->session);
    if (ctx->channel == NULL) return -1;

    if (ssh_channel_open_session(ctx->channel) != SSH_OK) {
        ssh_channel_free(ctx->channel);
        ctx->channel = NULL;
        return -1;
    }

    if (ssh_channel_request_pty(ctx->channel) != SSH_OK) {
        ssh_channel_close(ctx->channel);
        ssh_channel_free(ctx->channel);
        ctx->channel = NULL;
        return -1;
    }

    if (ssh_channel_change_pty_size(ctx->channel, 80, 24) != SSH_OK) {
        
    }

    if (ssh_channel_request_shell(ctx->channel) != SSH_OK) {
        ssh_channel_close(ctx->channel);
        ssh_channel_free(ctx->channel);
        ctx->channel = NULL;
        return -1;
    }

    return 0;
}

int ssh_read_nonblocking(SSHContext* ctx, char* buffer, size_t max_len) {
    if (!ctx || !ctx->channel) return -1;
    return ssh_channel_read_nonblocking(ctx->channel, buffer, max_len, 0);
}

int ssh_write_data(SSHContext* ctx, const char* buffer, size_t len) {
    if (!ctx || !ctx->channel) return -1;
    return ssh_channel_write(ctx->channel, buffer, len);
}

const char* ssh_get_error_msg(SSHContext* ctx) {
    if (!ctx || !ctx->session) return "No session";
    return ssh_get_error(ctx->session);
}

bool ssh_is_channel_open(SSHContext* ctx) {
    if (!ctx || !ctx->channel) return false;
    return ssh_channel_is_open(ctx->channel) && !ssh_channel_is_eof(ctx->channel);
}