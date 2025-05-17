#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <time.h>
#include <stdarg.h>

#define MAX_CHUNK_SIZE 1024
#define RELIC_DIR "/home/kali/baymax_fs/relics"
#define LOG_FILE "home/kali/baymax_fs/activity.log"

void log_activity(const char *fmt, ...) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);

    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);

    fprintf(fp, "\n");
    fclose(fp);
}

static int xmp_getattr(const char *path, struct stat *stbuf,
                       struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        char filename[256];
        strcpy(filename, path + 1);

        char chunk_path[512];
        sprintf(chunk_path, "%s/%s.000", RELIC_DIR, filename);

        if (access(chunk_path, F_OK) != 0)
            return -ENOENT;

        int chunk_num = 0;
        off_t total_size = 0;
        while (1) {
            sprintf(chunk_path, "%s/%s.%03d", RELIC_DIR, filename, chunk_num);
            FILE *fp = fopen(chunk_path, "rb");
            if (!fp) break;

            fseek(fp, 0, SEEK_END);
            total_size += ftell(fp);
            fclose(fp);
            chunk_num++;
        }

        stbuf->st_mode = S_IFREG | 0666;
        stbuf->st_nlink = 1;
        stbuf->st_size = total_size;
    }

    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    if (strcmp(path, "/") != 0)
        return 0;

    DIR *dp = opendir(RELIC_DIR);
    if (!dp) return -errno;

    struct dirent *de;
    char seen[1000][256];
    int seen_count = 0;

    while ((de = readdir(dp)) != NULL) {
        if (de->d_type != DT_REG) continue;

        char *dot = strrchr(de->d_name, '.');
        if (!dot || strlen(dot + 1) != 3) continue;

        char filename[256];
        strncpy(filename, de->d_name, dot - de->d_name);
        filename[dot - de->d_name] = '\0';

        int already_seen = 0;
        for (int i = 0; i < seen_count; i++) {
            if (strcmp(seen[i], filename) == 0) {
                already_seen = 1;
                break;
            }
        }

        if (!already_seen) {
            filler(buf, filename, NULL, 0, 0);
            strcpy(seen[seen_count++], filename);
        }
    }

    closedir(dp);
    return 0;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    return 0;
}

static int xmp_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
    char filename[256];
    strcpy(filename, path + 1);

    int chunk_num = 0;
    size_t written = 0;
    while (written < size) {
        char chunk_path[512];
        sprintf(chunk_path, "%s/%s.%03d", RELIC_DIR, filename, chunk_num);

        FILE *fp = fopen(chunk_path, "wb");
        if (!fp) return -errno;

        size_t to_write = (size - written > MAX_CHUNK_SIZE) ? MAX_CHUNK_SIZE : size - written;
        fwrite(buf + written, 1, to_write, fp);
        fclose(fp);

        written += to_write;
        chunk_num++;
    }

    log_activity("WRITE: %s -> %s.000 - %s.%03d", filename, filename, filename, chunk_num - 1);
    return size;
}

static int xmp_read(const char *path, char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    char filename[256];
    strcpy(filename, path + 1);

    size_t total_read = 0;
    int chunk_num = 0;

    while (1) {
        char chunk_path[512];
        sprintf(chunk_path, "%s/%s.%03d", RELIC_DIR, filename, chunk_num);
        FILE *fp = fopen(chunk_path, "rb");
        if (!fp) break;

        size_t read_size = fread(buf + total_read, 1, MAX_CHUNK_SIZE, fp);
        fclose(fp);

        total_read += read_size;
        if (total_read >= size) break;

        chunk_num++;
    }

    return total_read;
}

static int xmp_unlink(const char *path) {
    char filename[256];
    strcpy(filename, path + 1);

    int chunk_num = 0;
    char chunk_path[512];

    while (1) {
        sprintf(chunk_path, "%s/%s.%03d", RELIC_DIR, filename, chunk_num);
        if (access(chunk_path, F_OK) != 0)
            break;
        remove(chunk_path);
        chunk_num++;
    }

    if (chunk_num > 0)
        log_activity("DELETE: %s.000 - %s.%03d", filename, filename, chunk_num - 1);

    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    char filename[256];
    strcpy(filename, path + 1);
    log_activity("READ: %s", filename);
    return 0;
}

static struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .create  = xmp_create,
    .write   = xmp_write,
    .read    = xmp_read,
    .unlink  = xmp_unlink,
    .open    = xmp_open,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &xmp_oper, NULL);
}