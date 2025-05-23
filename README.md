```
===================================================================
Ananda Widi Alrafi 5027241067
Raynard Carlent 5027241109
Zahra Hafizhah 5027241121
===================================================================
```
No.1 
Pada soal ini kita disuruh untuk menkonversi file .txt untuk menjadi sebuah gambar .png dengan cara menggunakan FUSE 
```c
#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>

#define SOURCE_DIR "./anomali"
#define IMAGE_DIR "./image"
#define LOG_FILE "conversion.log"
```
```c
void get_timestamp(char *buffer, size_t size, const char *format) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, format, t);
}
```
Fungsi ini membantu membuat timestamp dalam bentuk string, yang berguna untuk penamaan file, logging, atau keperluan pencatatan waktu lainnya
```c
int convert_hex_to_image(const char *hex_path, const char *original_filename) {
    FILE *hex_file = fopen(hex_path, "r");
    if (!hex_file) return -1;

    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp), "%Y-%m-%d_%H:%M:%S");

    char base_name[256];
    strncpy(base_name, original_filename, sizeof(base_name));
    base_name[sizeof(base_name) - 1] = '\0';
    char *dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    char image_filename[512];
    snprintf(image_filename, sizeof(image_filename), "%s_image_%s.png", base_name, timestamp);

    char image_path[1024];
    snprintf(image_path, sizeof(image_path), "%s/%s", IMAGE_DIR, image_filename);

    FILE *img_file = fopen(image_path, "wb");
    if (!img_file) {
        fclose(hex_file);
        return -1;
    }

    char hexbuf[4096];
    unsigned char binbuf[2048];
    size_t hexlen = 0, i = 0, binlen = 0;
    int leftover = 0;

    while (!feof(hex_file)) {
        hexlen = fread(hexbuf + leftover, 1, sizeof(hexbuf) - leftover, hex_file);
        hexlen += leftover;
        if (hexlen == 0) break;
        binlen = 0;
        for (i = 0; i + 1 < hexlen; i += 2) {
            unsigned int byte;
            if (sscanf(&hexbuf[i], "%2x", &byte) == 1) {
                binbuf[binlen++] = (unsigned char)byte;
            }
        }
        fwrite(binbuf, 1, binlen, img_file);
        if (hexlen % 2 != 0) {
            hexbuf[0] = hexbuf[hexlen - 1];
            leftover = 1;
        } else {
            leftover = 0;
        }
    }

    fclose(hex_file);
    fclose(img_file);

    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        char log_time[32];
        get_timestamp(log_time, sizeof(log_time), "%Y-%m-%d][%H:%M:%S");
        fprintf(log, "[%s]: Successfully converted hexadecimal text %s to %s.\n", log_time, original_filename, image_filename);
        fclose(log);
    }

    return 0;
}
```
Fungsi convert_hex_to_image mengubah file teks heksadesimal menjadi file gambar .png dengan nama bertimestamp, menyimpannya di folder ./image, dan mencatat proses konversi ke dalam file log

```c
static int hexed_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%s", SOURCE_DIR, path);
    return lstat(full_path, st);
}
```c
static int hexed_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%s", SOURCE_DIR, path);
    return lstat(full_path, st);
}
```
Fungsi hexed_getattr mengambil atribut file (seperti ukuran dan waktu modifikasi) dari path asli di folder ./anomali menggunakan lstat

```c
static int hexed_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    DIR *dp;
    struct dirent *de;

    dp = opendir(SOURCE_DIR);
    if (!dp) return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    while ((de = readdir(dp)) != NULL) {
        filler(buf, de->d_name, NULL, 0, 0);
    }

    closedir(dp);
    return 0;
}
```
Fungsi hexed_readdir menampilkan daftar isi direktori ./anomali ke dalam filesystem FUSE, termasuk entri khusus "." dan ".."

```c
static int hexed_open(const char *path, struct fuse_file_info *fi) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%s", SOURCE_DIR, path);
    int res = open(full_path, O_RDONLY);
    if (res == -1) return -ENOENT;
    close(res);
    return 0;
}
```
Fungsi hexed_open memeriksa apakah file pada path di direktori ./anomali bisa dibuka untuk dibaca, dan mengembalikan kesalahan jika file tidak ditemukan

```c
static int hexed_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%s", SOURCE_DIR, path);

    const char *filename = path[0] == '/' ? path + 1 : path;
    convert_hex_to_image(full_path, filename);

    int fd = open(full_path, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;

    close(fd);
    return res;
}
```
Fungsi hexed_read membaca isi file dari direktori ./anomali, terlebih dahulu mengonversi file tersebut menjadi gambar menggunakan convert_hex_to_image, lalu mengembalikan data file yang dibaca

```c
static const struct fuse_operations hexed_oper = {
    .getattr = hexed_getattr,
    .readdir = hexed_readdir,
    .open = hexed_open,
    .read = hexed_read,
};
```
Struktur hexed_oper mendefinisikan operasi-operasi FUSE yang digunakan filesystem ini, yaitu untuk mengambil atribut file (getattr), membaca isi direktori (readdir), membuka file (open), dan membaca file (read)

```c
int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &hexed_oper, NULL);
}
```
Fungsi main menjalankan filesystem FUSE dengan operasi yang didefinisikan dalam hexed_oper, menggunakan argumen dari baris perintah

No.2
Pada nomor 2 ini, kita diminta untuk membuat sistem file virtual menggunakan FUSE yang menyatukan file Baymax.jpeg yang terpecah menjadi 14 bagian dalam folder relics, sehingga muncul sebagai satu file utuh di direktori mount_dir. Selain itu, sistem harus bisa memecah file baru yang ditulis ke dalam potongan 1 KB dan menyimpannya di relics, menghapus semua pecahan saat file dihapus, serta mencatat semua aktivitas (read, write, copy, delete) ke dalam activity.log.

1. Global Constants
```c
#define MAX_CHUNK_SIZE 1024
#define RELIC_DIR "/home/kali/baymax_fs/relics"
#define LOG_FILE "home/kali/baymax_fs/activity.log"
```
- Menentukan ukuran maksimal setiap potongan file (1 KB).
- RELIC_DIR: Lokasi folder penyimpanan pecahan file.
- LOG_FILE: Lokasi file log aktivitas.

2. log_activity
```c
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
```
Mencatat log aktivitas ke file activity.log dengan format timestamp.

3. xmp_getattr
```c
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
```
Mengembalikan informasi atribut file atau direktori.

4. xmp_readdir
```c
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
```
Membaca isi direktori mount dan menampilkan file hasil penyatuan pecahan.

5. xmp_create
```c
static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    return 0;
}
```
Stub untuk membuat file baru, implementasi logika ada di write.

6. xmp_write
```c
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

```
Menulis file ke direktori mount, memecah menjadi potongan 1KB, disimpan di relics.

7. xmp_read
```c
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

```
Membaca gabungan isi dari semua potongan dan mengisi buffer pembaca.

8. xmp_unlink
```c
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
```
Menghapus file virtual (di mount), sekaligus semua pecahan fisiknya di relics.

9. xmp_open
```c
static int xmp_open(const char *path, struct fuse_file_info *fi) {
    char filename[256];
    strcpy(filename, path + 1);
    log_activity("READ: %s", filename);
    return 0;
}
```
Menangani pembukaan file, mencatat log aktivitas READ.

10. Main
```c
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
```
FUSE entry point, mendaftarkan semua operasi ke FUSE.


No.3
Sistem ini merupakan implementasi Filesystem in Userspace (FUSE) dalam container Docker yang:
-Mendeteksi file dengan kata kunci "nafis" atau "kimcun"
-Membalik nama file berbahaya saat ditampilkan
-Mengenkripsi isi file normal dengan ROT13
-Mencatat semua aktivitas dalam log file


 - Docker File
```DockerFile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    fuse \
    libfuse-dev \
    pkg-config \
    gcc \
    make \
    openssl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY antink.c .
RUN gcc -Wall antink.c `pkg-config fuse --cflags --libs` -o antink

RUN mkdir -p /it24_host /antink_mount /var/log

CMD ["/app/antink", "/antink_mount"]
```

-Docker-compose.yml
```c
version: '3'

services:
  antink-server:
    build: .
    container_name: antink-server
    privileged: true
    volumes:
      - ./it24_host:/it24_host:ro
      - ./antink_mount:/antink_mount
      - ./antink-logs:/var/log
    cap_add:
      - SYS_ADMIN
    devices:
      - /dev/fuse

  antink-logger:
    image: ubuntu:22.04
    container_name: antink-logger
    command: tail -f /var/log/it24.log
    volumes:
      - ./antink-logs:/var/log
    depends_on:
      - antink-server
```

-antink.c (Implementasi FUSE)
```c
#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <openssl/aes.h>

#define MAX_PATH 1024
#define LOG_FILE "/var/log/it24.log"
#define HOST_DIR "/it24_host"

// Fungsi untuk menulis log
void write_log(const char *action, const char *details) {
    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    
    char timestamp[20];
    strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", tm_info);
    
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file) {
        fprintf(log_file, "[%s] %s: %s\n", timestamp, action, details);
        fclose(log_file);
    }
}

// Fungsi untuk membalik string
void reverse_string(char *str) {
    if (!str) return;
    
    int length = strlen(str);
    for (int i = 0; i < length / 2; i++) {
        char temp = str[i];
        str[i] = str[length - i - 1];
        str[length - i - 1] = temp;
    }
}

// Fungsi untuk memeriksa apakah file berbahaya
int is_dangerous(const char *filename) {
    return strstr(filename, "nafis") || strstr(filename, "kimcun");
}

// Fungsi ROT13
void rot13(char *str) {
    for (; *str; str++) {
        if (isalpha(*str)) {
            if ((*str >= 'a' && *str <= 'm') || (*str >= 'A' && *str <= 'M')) {
                *str += 13;
            } else {
                *str -= 13;
            }
        }
    }
}

// Implementasi FUSE operations
static int antink_getattr(const char *path, struct stat *stbuf) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", HOST_DIR, path);
    
    int res = lstat(full_path, stbuf);
    if (res == -1) return -errno;
    
    return 0;
}

static int antink_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", HOST_DIR, path);
    
    DIR *dp = opendir(full_path);
    if (!dp) return -errno;
    
    struct dirent *de;
    while ((de = readdir(dp)) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        
        // Balik nama jika berbahaya
        char display_name[256];
        strcpy(display_name, de->d_name);
        
        if (is_dangerous(display_name)) {
            reverse_string(display_name);
        }
        
        if (filler(buf, display_name, &st, 0)) break;
    }
    
    closedir(dp);
    return 0;
}

static int antink_open(const char *path, struct fuse_file_info *fi) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", HOST_DIR, path);
    
    int res = open(full_path, fi->flags);
    if (res == -1) return -errno;
    
    close(res);
    
    // Log aktivitas baca
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "READ: %s", path);
    write_log("READ", log_msg);
    
    return 0;
}

static int antink_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", HOST_DIR, path);
    
    int fd = open(full_path, O_RDONLY);
    if (fd == -1) return -errno;
    
    int res = pread(fd, buf, size, offset);
    if (res == -1) {
        close(fd);
        return -errno;
    }
    
    // Enkripsi ROT13 hanya untuk file teks normal
    if (!is_dangerous(path) && strstr(path, ".txt")) {
        rot13(buf);
    }
    
    close(fd);
    return res;
}

static int antink_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", HOST_DIR, path);
    
    int fd = open(full_path, fi->flags, mode);
    if (fd == -1) return -errno;
    
    close(fd);
    
    // Log aktivitas buat file
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "CREATE: %s", path);
    write_log("CREATE", log_msg);
    
    return 0;
}

static int antink_write(const char *path, const char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", HOST_DIR, path);
    
    int fd = open(full_path, O_WRONLY);
    if (fd == -1) return -errno;
    
    int res = pwrite(fd, buf, size, offset);
    if (res == -1) {
        close(fd);
        return -errno;
    }
    
    close(fd);
    
    // Log aktivitas tulis
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "WRITE: %s", path);
    write_log("WRITE", log_msg);
    
    return res;
}

static int antink_unlink(const char *path) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", HOST_DIR, path);
    
    int res = unlink(full_path);
    if (res == -1) return -errno;
    
    // Log aktivitas hapus
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "DELETE: %s", path);
    write_log("DELETE", log_msg);
    
    return 0;
}

static struct fuse_operations antink_oper = {
    .getattr = antink_getattr,
    .readdir = antink_readdir,
    .open = antink_open,
    .read = antink_read,
    .create = antink_create,
    .write = antink_write,
    .unlink = antink_unlink,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &antink_oper, NULL);
}
```

Untuk build dan jalankan container dapat menggunakan command
```c
docker-compose build --no-cache  
docker-compose up -d
```

Sebenernya, harus ada file contoh untuk dapat menjalankan program seperti file  txt ini
```c
echo "Ini file normal" > soal_3/it24_host/normal.txt
echo "Ini file nafis" > soal_3/it24_host/nafis.txt
echo "Ini file kimcun" > soal_3/it24_host/kimcun.txt
```

Nah di soal ketiga aku belum bisa berhasil menjalankan program yang ada dikarenakan ada beberapa kesalahan. 
