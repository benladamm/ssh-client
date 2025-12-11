#include "ssh_sftp.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

SFTPContext* sftp_context_new(SSHContext *ssh_ctx) {
    if (!ssh_ctx || !ssh_ctx->session) return NULL;
    
    SFTPContext *ctx = malloc(sizeof(SFTPContext));
    ctx->ssh_ctx = ssh_ctx;
    ctx->sftp = NULL;
    ctx->is_initialized = false;
    return ctx;
}

int sftp_init_session(SFTPContext *ctx) {
    if (!ctx || !ctx->ssh_ctx) return -1;
    
    ctx->sftp = sftp_new(ctx->ssh_ctx->session);
    if (!ctx->sftp) return -1;
    
    if (sftp_init(ctx->sftp) != SSH_OK) {
        sftp_free(ctx->sftp);
        ctx->sftp = NULL;
        return -1;
    }
    
    ctx->is_initialized = true;
    return 0;
}

void sftp_context_free(SFTPContext *ctx) {
    if (ctx) {
        if (ctx->sftp) {
            sftp_free(ctx->sftp);
        }
        free(ctx);
    }
}

static char* format_permissions(mode_t mode) {
    char *p = malloc(11);
    strcpy(p, "----------");
    if (S_ISDIR(mode)) p[0] = 'd';
    if (S_ISLNK(mode)) p[0] = 'l';
    if (mode & S_IRUSR) p[1] = 'r';
    if (mode & S_IWUSR) p[2] = 'w';
    if (mode & S_IXUSR) p[3] = 'x';
    if (mode & S_IRGRP) p[4] = 'r';
    if (mode & S_IWGRP) p[5] = 'w';
    if (mode & S_IXGRP) p[6] = 'x';
    if (mode & S_IROTH) p[7] = 'r';
    if (mode & S_IWOTH) p[8] = 'w';
    if (mode & S_IXOTH) p[9] = 'x';
    return p;
}

SFTPFile** sftp_list_directory(SFTPContext *ctx, const char *path, int *count) {
    if (!ctx || !ctx->sftp || !path) {
        *count = 0;
        return NULL;
    }
    
    sftp_dir dir = sftp_opendir(ctx->sftp, path);
    if (!dir) return NULL;
    
    sftp_attributes attributes;
    int capacity = 20;
    int size = 0;
    SFTPFile **files = malloc(sizeof(SFTPFile*) * capacity);
    
    while ((attributes = sftp_readdir(ctx->sftp, dir)) != NULL) {
        if (size >= capacity) {
            capacity *= 2;
            files = realloc(files, sizeof(SFTPFile*) * capacity);
        }
        
        SFTPFile *f = malloc(sizeof(SFTPFile));
        f->name = strdup(attributes->name);
        f->size = attributes->size;
        f->mtime = attributes->mtime;
        
        if (attributes->type == SSH_FILEXFER_TYPE_DIRECTORY) {
            f->type = SFTP_TYPE_DIRECTORY;
        } else if (attributes->type == SSH_FILEXFER_TYPE_SYMLINK) {
            f->type = SFTP_TYPE_SYMLINK;
        } else {
            f->type = SFTP_TYPE_REGULAR;
        }
        
        f->permissions = format_permissions(attributes->permissions);
        
        sftp_attributes_free(attributes);
        files[size++] = f;
    }
    
    sftp_closedir(dir);
    *count = size;
    return files;
}

void sftp_free_file_list(SFTPFile **files, int count) {
    if (!files) return;
    for (int i = 0; i < count; i++) {
        free(files[i]->name);
        free(files[i]->permissions);
        free(files[i]);
    }
    free(files);
}

int sftp_download_file(SFTPContext *ctx, const char *remote_path, const char *local_path) {
    if (!ctx || !ctx->sftp) return -1;
    
    sftp_file file = sftp_open(ctx->sftp, remote_path, O_RDONLY, 0);
    if (!file) return -1;
    
    int fd = open(local_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        sftp_close(file);
        return -1;
    }
    
    char buffer[16384];
    int nbytes, rc = 0;
    while ((nbytes = sftp_read(file, buffer, sizeof(buffer))) > 0) {
        if (write(fd, buffer, nbytes) != nbytes) {
            rc = -1;
            break;
        }
    }
    
    close(fd);
    sftp_close(file);
    return rc;
}

int sftp_upload_file(SFTPContext *ctx, const char *local_path, const char *remote_path) {
    if (!ctx || !ctx->sftp) return -1;
    
    int fd = open(local_path, O_RDONLY);
    if (fd < 0) return -1;
    
    sftp_file file = sftp_open(ctx->sftp, remote_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!file) {
        close(fd);
        return -1;
    }
    
    char buffer[16384];
    int nbytes, rc = 0;
    while ((nbytes = read(fd, buffer, sizeof(buffer))) > 0) {
        if (sftp_write(file, buffer, nbytes) != nbytes) {
            rc = -1;
            break;
        }
    }
    
    sftp_close(file);
    close(fd);
    return rc;
}

int sftp_create_directory(SFTPContext *ctx, const char *path) {
    if (!ctx || !ctx->sftp) return -1;
    return sftp_mkdir(ctx->sftp, path, 0755);
}

int sftp_delete_file(SFTPContext *ctx, const char *path) {
    if (!ctx || !ctx->sftp) return -1;
    return sftp_unlink(ctx->sftp, path);
}

char* sftp_get_cwd(SFTPContext *ctx) {
     if (!ctx || !ctx->sftp) return NULL;
     return sftp_canonicalize_path(ctx->sftp, ".");
}
