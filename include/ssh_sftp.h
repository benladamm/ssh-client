#ifndef SSH_SFTP_H
#define SSH_SFTP_H

#include "ssh_backend.h"
#include <libssh/sftp.h>

// File types
typedef enum {
    SFTP_TYPE_UNKNOWN = 0,
    SFTP_TYPE_REGULAR,
    SFTP_TYPE_DIRECTORY,
    SFTP_TYPE_SYMLINK
} SFTPFileType;

typedef struct {
    char *name;
    SFTPFileType type;
    uint64_t size;
    uint64_t mtime;
    char *permissions;
} SFTPFile;

typedef struct {
    SSHContext *ssh_ctx;
    sftp_session sftp;
    bool is_initialized;
} SFTPContext;

SFTPContext* sftp_context_new(SSHContext *ssh_ctx);

int sftp_init_session(SFTPContext *ctx);

void sftp_context_free(SFTPContext *ctx);

SFTPFile** sftp_list_directory(SFTPContext *ctx, const char *path, int *count);

void sftp_free_file_list(SFTPFile **files, int count);

int sftp_download_file(SFTPContext *ctx, const char *remote_path, const char *local_path);

int sftp_upload_file(SFTPContext *ctx, const char *local_path, const char *remote_path);

int sftp_create_directory(SFTPContext *ctx, const char *path);

int sftp_delete_file(SFTPContext *ctx, const char *path);

char* sftp_get_cwd(SFTPContext *ctx);

#endif
