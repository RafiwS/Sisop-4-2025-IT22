#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#define OUTPUT_SUBDIR "image"
#define LOG_FILENAME "conversion.log"

void get_timestamp(char *date_str, char *time_str) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(date_str, 11, "%Y-%m-%d", t);
    strftime(time_str, 9, "%H:%M:%S", t);
}

int hexchar_to_int(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

void convert_file(const char *input_dir, const char *filename) {
    char input_path[512];
    snprintf(input_path, sizeof(input_path), "%s/%s", input_dir, filename);
    FILE *in = fopen(input_path, "r");
    if (!in) {
        perror("Open input failed");
        return;
    }

    char date_str[11], time_str[9];
    get_timestamp(date_str, time_str);

    char output_dir[512];
    snprintf(output_dir, sizeof(output_dir), "%s/%s", input_dir, OUTPUT_SUBDIR);
    mkdir(output_dir, 0755);

    char base_name[128];
    strncpy(base_name, filename, sizeof(base_name) - 1);
    base_name[sizeof(base_name) - 1] = '\0';
    char *dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    char output_path[1024];
    snprintf(output_path, sizeof(output_path),
             "%s/%s_image_%s_%s.png", output_dir, base_name, date_str, time_str);
    FILE *out = fopen(output_path, "wb");
    if (!out) {
        perror("Failed to open output");
        fclose(in);
        return;
    }

    int hi = -1;
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (isspace(c)) continue;
        int val = hexchar_to_int(c);
        if (val == -1) continue;

        if (hi == -1) {
            hi = val;
        } else {
            unsigned char byte = (hi << 4) | val;
            fwrite(&byte, 1, 1, out);
            hi = -1;
        }
    }

    fclose(in);
    fclose(out);

    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/%s", input_dir, LOG_FILENAME);
    FILE *logf = fopen(log_path, "a");
    if (logf) {
        fprintf(logf, "[%s][%s]: Successfully converted hexadecimal text %s to %s.\n",
                date_str, time_str, filename, strrchr(output_path, '/') + 1);
        fclose(logf);
    }

    printf("SUCCESS: %s -> %s\n", filename, output_path);
}

int main(int argc, char *argv[]) {
    const char *input_dir = (argc > 1) ? argv[1] : "anomali";

    DIR *d = opendir(input_dir);
    if (!d) {
        perror("Failed to open directory");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(d))) {
        if (entry->d_type == DT_REG) {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".txt") == 0) {
                convert_file(input_dir, entry->d_name);
            }
        }
    }

    closedir(d);
    return 0;
}
