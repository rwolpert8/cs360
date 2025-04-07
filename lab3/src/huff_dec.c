#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_STRING_LENGTH 10000

struct huff_node {
    struct huff_node *ptrs[2];
    char *strings[2];
};

// Create a new Huffman node
struct huff_node* create_node() {
    struct huff_node *node = (struct huff_node*)malloc(sizeof(struct huff_node));
    node->ptrs[0] = node->ptrs[1] = NULL;
    node->strings[0] = node->strings[1] = NULL;
    return node;
}

// Insert a string and its corresponding code into the Huffman tree
void insert_code(struct huff_node *root, const char *string, const char *code) {
    struct huff_node *current = root;
    int last_bit = 0; // Track the final bit of the code
    while (*code) {
        int bit = *code - '0';
        last_bit = bit; // Update last_bit
        if (current->ptrs[bit] == NULL) {
            current->ptrs[bit] = create_node();
        }
        current = current->ptrs[bit];
        code++;
    }
    current->strings[last_bit] = strdup(string);
}

// Build the Huffman tree from the code definition file
void build_tree(struct huff_node *root, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error opening code definition file.\n");
        exit(1);
    }

    char string[MAX_STRING_LENGTH];
    char code[MAX_STRING_LENGTH];

    while (1) {
        // Read "string" until '\0' or EOF
        int c = fgetc(file);
        if (c == EOF) break;
        int i = 0;
        while (c != '\0' && c != EOF) {
            string[i++] = (char)c;
            c = fgetc(file);
        }
        string[i] = '\0';
        // If we hit EOF while reading string
        if (c == EOF) break;

        // Read "code" until '\0' or EOF
        c = fgetc(file);
        if (c == EOF) break;
        i = 0;
        while (c != '\0' && c != EOF) {
            code[i++] = (char)c;
            c = fgetc(file);
        }
        code[i] = '\0';
        if (c == EOF) break;

        // Insert into Huffman tree
        insert_code(root, string, code);
    }

    fclose(file);
}

void decode_file(struct huff_node *root, const char *filename) {
   // Open the input file
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error opening input file.\n");
        exit(1);
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    
    // Read the declared bit_count
    fseek(file, -4, SEEK_END);
    unsigned int bit_count;
    if (fread(&bit_count, sizeof(unsigned int), 1, file) != 1) {
        fprintf(stderr, "Error: file is not the correct size.\n");
        fclose(file);
        exit(1);
    }

    // Calculate required size = data bytes for bit_count + 4 bytes for bit_count
    long required_size = ((bit_count + 7) / 8) + 4;

    // Check if the file size matches the required size
    if (required_size != file_size) {
        fprintf(stderr, "Error: Total bits = %u, but file's size is %ld\n", bit_count, file_size);
        fclose(file);
        exit(1);
    }

    // Go back to the start of the file
    fseek(file, 0, SEEK_SET);

    unsigned int bits_processed = 0;
    struct huff_node *current = root;

    // Read one byte at a time until we process all required bits
    while (bits_processed < bit_count) {
        unsigned char byte;
        if (fread(&byte, 1, 1, file) != 1) {
            fprintf(stderr, "Error reading input file.\n");
            fclose(file);
            exit(1);
        }

        // Process up to 8 bits in this byte
        for (int bit = 0; bit < 8; bit++) {
            if (bits_processed >= bit_count) break;
            int bit_value = (byte >> bit) & 1;

            // Move down the Huffman tree
            if (current->ptrs[bit_value]) {
                current = current->ptrs[bit_value];

                // If we're at a leaf node (strings[0] or strings[1] is set), print it
                if (current->strings[0] || current->strings[1]) {
                    if (current->strings[0]) {
                        printf("%s", current->strings[0]);
                    } else {
                        printf("%s", current->strings[1]);
                    }
                    // Reset to the root to decode the next symbol
                    current = root;
                }
            } else {
                // No child for this bit = invalid sequence
                fprintf(stderr, "Unrecognized bits\n");
                fclose(file);
                exit(1);
            }
            bits_processed++;
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <code definition file> <input file>\n", argv[0]);
        return 1;
    }

    // Create the root of the Huffman tree
    struct huff_node *root = create_node();
    build_tree(root, argv[1]);
    decode_file(root, argv[2]);

    return 0;
}