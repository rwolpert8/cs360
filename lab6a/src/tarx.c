/*  CS 360 Lab 6: Tarx
    Name: Ryan Wolpert
    Student ID: 000499029
    Description: This program simulates a tar extraction system.
*/

/* Note: I am aware that I am turning this in a day late with only 60/100 gradescripts passing.
   The gradescripts that are failing are every one ending in a 5-9 starting with 15. I believe
   it is an issue with my hard link generation that is causing errors. I unfortunately have to
   turn in in this state so I can move on to other things. */

   #include "dllist.h"
   #include "jval.h"
   #include "jrb.h"
   #include "fields.h"
   #include <stdio.h>
   #include <stdint.h>
   #include <stdlib.h>
   #include <string.h>
   #include <sys/stat.h>
   #include <sys/time.h>
   #include <unistd.h>
   #include <fcntl.h>
   #include <endian.h>
   #include <errno.h>
   
// Global list for directories
typedef struct {
    char *path;
    time_t mtime;
    mode_t mode;
} DirInfo;

JRB inodeTree = NULL;
DirInfo *dirList = NULL;
size_t dirCount = 0;

// Add directory info to global list
void addDirectory(const char *path, time_t mtime, mode_t mode) {
    dirList = realloc(dirList, (dirCount + 1) * sizeof(DirInfo));
    if (!dirList) {
        perror("realloc");
        exit(1);
    }
    dirList[dirCount].path = strdup(path);
    if (!dirList[dirCount].path) {
        perror("strdup");
        exit(1);
    }
    dirList[dirCount].mtime = mtime;
    dirList[dirCount].mode = mode;
    dirCount++;
}

// Update directory times after all files have been processed
void updateDirectoryTimes(void) {
    for (size_t i = 0; i < dirCount; i++) {
        // Set the final mode and then update the mtime
        chmod(dirList[i].path, dirList[i].mode);
        setFileMtime(dirList[i].path, dirList[i].mtime);
        free(dirList[i].path);
    }
    free(dirList);
}

// Read a 32-bit little-endian value
uint32_t readUint32LE(int fd) {
    uint32_t val;
    if (read(fd, &val, 4) != 4) {
        perror("read uint32");
        exit(1);
    }
    return le32toh(val);
}

// Read a 64-bit little-endian value
uint64_t readUint64LE(int fd) {
    uint64_t val;
    if (read(fd, &val, 8) != 8) {
        perror("read uint64");
        exit(1);
    }
    return le64toh(val);
}

// Set file mtime using utimes()
void setFileMtime(const char *path, time_t mtime) {
    struct timeval times[2];
    times[0].tv_sec = time(NULL);
    times[0].tv_usec = 0;
    times[1].tv_sec = mtime;
    times[1].tv_usec = 0;
    utimes(path, times);
}

// Extract tar archive entries exactly
void extractTarFile(void) {
    while (1) {
        // Read the 4-byte name length
        uint32_t nameLenLE;
        ssize_t n = read(STDIN_FILENO, &nameLenLE, 4);
        if (n == 0) {
            // EOF
            break;
        } else if (n < 4) {
            fprintf(stderr, "Truncated tar file.\n");
            break;
        }
        uint32_t nameLen = le32toh(nameLenLE);

        // Read nameLen bytes into name, then null-terminate
        char *name = malloc(nameLen + 1);
        if (!name) {
            perror("malloc");
            exit(1);
        }
        if (read(STDIN_FILENO, name, nameLen) != (ssize_t)nameLen) {
            fprintf(stderr, "Error reading file name\n");
            free(name);
            break;
        }
        name[nameLen] = '\0';

        //Read inode, mode, mtime
        uint64_t inode = readUint64LE(STDIN_FILENO);
        mode_t mode = (mode_t)readUint32LE(STDIN_FILENO);
        time_t mtime = (time_t)readUint64LE(STDIN_FILENO);

        // For regular files, read file size and then write exactly that many bytes
        if (!S_ISDIR(mode)) {
            // Read file size
            uint64_t fSize = readUint64LE(STDIN_FILENO);
            
            // Check for a previous occurrence of this inode
            JRB node = jrb_find_int(inodeTree, (int)inode);
            if (node) {
                // A file with this inode has already been extracted
                const char *existingPath = (const char *)jval_s(node->val);
                if (link(existingPath, name) < 0) {
                    fprintf(stderr, "Failed to create hard link %s to %s: %s\n",
                            name, existingPath, strerror(errno));
                }
                // Skip over file data in the stream
                char buffer[4096];
                uint64_t remaining = fSize;
                while (remaining > 0) {
                    ssize_t toRead = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);
                    ssize_t chunk = read(STDIN_FILENO, buffer, toRead);
                    if (chunk < 0) {
                        fprintf(stderr, "Error skipping file data: %s\n", strerror(errno));
                        break;
                    }
                    if (chunk == 0) {
                        // EOF reached unexpectedly
                        break;
                    }
                    remaining -= chunk;
                }
                if (remaining != 0) {
                    fprintf(stderr, "Error skipping file data: incomplete skip (remaining %llu bytes)\n", remaining);
                }
                
                // Restore mtime and final permissions
                setFileMtime(name, mtime);
                chmod(name, mode);
            } else {
                // Extract file data normally
                int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0777);
                if (fd < 0) {
                    fprintf(stderr, "Failed to create file %s: %s\n", name, strerror(errno));
                    free(name);
                    break;
                }
                uint64_t remaining = fSize;
                char buffer[4096];
                while (remaining > 0) {
                    ssize_t chunk = read(STDIN_FILENO, buffer, (remaining < sizeof(buffer)) ? remaining : sizeof(buffer));
                    if (chunk <= 0) {
                        fprintf(stderr, "Error reading file data\n");
                        break;
                    }
                    if (write(fd, buffer, chunk) != chunk) {
                        fprintf(stderr, "Error writing to file %s: %s\n", name, strerror(errno));
                        break;
                    }
                    remaining -= chunk;
                }
                close(fd);

                // Save the mapping from inode to pathname
                jrb_insert_int(inodeTree, (int)inode, new_jval_s(strdup(name)));

                // Restore file mtime and permissions
                setFileMtime(name, mtime);
                chmod(name, mode);
            }
        } else {
            // For directories, create them with permissive mode
            if (mkdir(name, 0777) < 0 && errno != EEXIST) {
                fprintf(stderr, "Failed to create directory %s: %s\n", name, strerror(errno));
            }
            addDirectory(name, mtime, mode);
        }
        free(name);
    }
}

int main(void) {
    inodeTree = make_jrb();
    extractTarFile();
    updateDirectoryTimes();
    return 0;
}