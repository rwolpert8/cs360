/*  CS 360 Lab 5: Tarc
    Name: Ryan Wolpert
    Student ID: 000499029
    Description: This program simulates a tar archive system.
*/

#include "dllist.h"
#include "jval.h"
#include "jrb.h"
#include "fields.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static JRB inode_map = NULL;

typedef struct {
    ino_t inode;
    mode_t mode;
    off_t size;
    time_t mtime;
} FileData;

// Helper to write a 32-bit little-endian value to stdout
static void write_le32(uint32_t val) {
    unsigned char buf[4];
    buf[0] = (unsigned char)(val & 0xff);
    buf[1] = (unsigned char)((val >> 8) & 0xff);
    buf[2] = (unsigned char)((val >> 16) & 0xff);
    buf[3] = (unsigned char)((val >> 24) & 0xff);
    fwrite(buf, 1, 4, stdout);
}

// Helper to write a 64-bit little-endian value to stdout
static void write_le64(uint64_t val) {
    unsigned char buf[8];
    buf[0] = (unsigned char)(val & 0xff);
    buf[1] = (unsigned char)((val >> 8) & 0xff);
    buf[2] = (unsigned char)((val >> 16) & 0xff);
    buf[3] = (unsigned char)((val >> 24) & 0xff);
    buf[4] = (unsigned char)((val >> 32) & 0xff);
    buf[5] = (unsigned char)((val >> 40) & 0xff);
    buf[6] = (unsigned char)((val >> 48) & 0xff);
    buf[7] = (unsigned char)((val >> 56) & 0xff);
    fwrite(buf, 1, 8, stdout);
}

static void write_file_entry(const char *tarName, const struct stat *st, int is_new_inode) {
    // Step 1: Write length of tarName (4 bytes, little-endian)
    uint32_t nameLen = (uint32_t)strlen(tarName);
    write_le32(nameLen);

    // Step 2: Write tarName (no null terminator)
    fwrite(tarName, 1, nameLen, stdout);

    // Step 3: Write st_ino (8 bytes, little-endian)
    write_le64((uint64_t)st->st_ino);

    // If this is a new inode, also write mode (4 bytes) and mtime (8 bytes)
    if (is_new_inode) {
        write_le32((uint32_t)st->st_mode);
        write_le64((uint64_t)st->st_mtime);
    }

    // If a regular file and it’s a new inode, also write size and contents
    if (S_ISREG(st->st_mode) && is_new_inode) {
        write_le64((uint64_t)st->st_size);
    }
}

// Using a stack-based approach to process directories
static void process_directory(const char *path, const char *relative) {
    Dllist stack = new_dllist();
    dll_append(stack, new_jval_s(strdup(path)));
    dll_append(stack, new_jval_s(strdup(relative)));

    while (!dll_empty(stack)) {
        // Pop the top directory from the stack
        Dllist node = dll_last(stack);
        char *current_relative = strdup(jval_s(dll_val(node)));
        dll_delete_node(node);

        // Pop the corresponding path
        node = dll_last(stack);
        char *current_path = strdup(jval_s(dll_val(node)));
        dll_delete_node(node);

        // Open the directory
        DIR *dp = opendir(current_path);
        if (!dp) {
            perror("opendir failed");
            free(current_path);
            free(current_relative);
            continue;
        }

        // Read entries in the directory
        struct dirent *entry;
        while ((entry = readdir(dp))) {
            // Ignore "." and ".."
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
                continue;
            }

            // Build paths
            char fullPath[1024];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", current_path, entry->d_name);

            // Decide how the tar name should look
            char tarName[1024];
            snprintf(tarName, sizeof(tarName), "%s/%s", current_relative, entry->d_name);

            // Get file info
            struct stat st;
            if (lstat(fullPath, &st) < 0) {
                perror("lstat failed");
                closedir(dp);
                free(current_path);
                free(current_relative);
                exit(1);
            }

            // Ignore symbolic links
            if (S_ISLNK(st.st_mode)) {
                continue;
            }

            // Check if we’ve seen this inode before
            JRB node = jrb_find_int(inode_map, (int)st.st_ino);
            int is_new_inode = 0;
            if (!node) {
                // Create new FileData for this inode
                FileData *newFile = malloc(sizeof(FileData));
                newFile->inode = st.st_ino;
                newFile->mode = st.st_mode;
                newFile->mtime = st.st_mtime;
                newFile->size = st.st_size;

                jrb_insert_int(inode_map, (int)st.st_ino, new_jval_v((void*)newFile));
                is_new_inode = 1;
            } 

            // Write the file entry
            write_file_entry(tarName, &st, is_new_inode);

            // If directory, push it onto the stack
            if (S_ISDIR(st.st_mode)) {
                dll_append(stack, new_jval_s(strdup(fullPath)));
                dll_append(stack, new_jval_s(strdup(tarName)));
            }
            // If regular file, write out its contents only if it's a new inode
            else if (S_ISREG(st.st_mode) && is_new_inode) {
                FILE *fp = fopen(fullPath, "rb");
                if (!fp) {
                    perror("fopen failed");
                    closedir(dp);
                    free(current_path);
                    free(current_relative);
                    exit(1);
                }
                char buf[4096];
                size_t bytes;
                while ((bytes = fread(buf, 1, sizeof(buf), fp)) > 0) {
                    fwrite(buf, 1, bytes, stdout);
                }
                fclose(fp);
            }
        }
        closedir(dp);
        free(current_path);
        free(current_relative);
    }
    free_dllist(stack);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    // Extract base name (leaf) from the directory path
    const char *base = strrchr(argv[1], '/');
    if (base)
        base++;  // skip the '/'
    else
        base = argv[1];

    inode_map = make_jrb();
    
    // Write tar entry for the top-level directory
    struct stat st;
    if (lstat(argv[1], &st) < 0) {
        perror("lstat failed");
        exit(1);
    }
    write_file_entry(base, &st, 1);

    process_directory(argv[1], base);

    // Cleanup
    jrb_free_tree(inode_map);
    return 0;
}