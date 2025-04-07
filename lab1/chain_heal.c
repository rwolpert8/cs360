/*  CS 360 Lab 1: Chain Heal
    Name: Ryan Wolpert
    Student ID: 000499029
    Description: This program simulates a chain heal effect in a game.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>


typedef struct node {
    char name[101];
    int x, y;
    int cur_PP, max_PP;
    int adj_size;
    struct node **adj;
    int visited;
    int healing;
    struct node *prev;
} Node;

Node **nodes;
int num_nodes = 0;

void read_input() {
    char name[101];
    int x, y, cur_PP, max_PP;
    while (scanf("%d %d %d %d %100s", &x, &y, &cur_PP, &max_PP, name) == 5) {
        Node *new_node = (Node *)malloc(sizeof(Node));
        strcpy(new_node->name, name);
        new_node->x = x;
        new_node->y = y;
        new_node->cur_PP = cur_PP;
        new_node->max_PP = max_PP;
        new_node->adj_size = 0;
        new_node->adj = NULL;
        new_node->visited = 0;
        new_node->healing = 0;
        new_node->prev = NULL;
        nodes[num_nodes++] = new_node;
    }
}

// Helper function to calculate distance between two nodes
double distance(Node *a, Node *b) {
    return sqrt(pow(a->x - b->x, 2) + pow(a->y - b->y, 2));
}

// Helper function to compare nodes by name
int compare_nodes(const void *a, const void *b) {
    Node *na = *(Node **)a;
    Node *nb = *(Node **)b;
    return strcmp(na->name, nb->name);
}

// Create the adjacency list for the graph
void create_graph(int jump_range) {
    for (int i = 0; i < num_nodes; i++) {
        for (int j = 0; j < num_nodes; j++) {
            if (i != j && distance(nodes[i], nodes[j]) <= jump_range) {
                nodes[i]->adj_size++;
            }
        }
        // Allocate memory for adjacency list
        nodes[i]->adj = (Node **)malloc(sizeof(Node *) * nodes[i]->adj_size);
        nodes[i]->adj_size = 0;
    }
    // Fill adjacency list
    for (int i = 0; i < num_nodes; i++) {
        for (int j = 0; j < num_nodes; j++) {
            if (i != j && distance(nodes[i], nodes[j]) <= jump_range) {
                nodes[i]->adj[nodes[i]->adj_size++] = nodes[j];
            }
        }
    }
    // Sort adjacency lists for deterministic DFS order
    for (int i = 0; i < num_nodes; i++) {
        qsort(nodes[i]->adj, nodes[i]->adj_size, sizeof(Node *), compare_nodes);
    }
}

// DFS traversal to find the path with the most healing
void dfs(Node *node, int hop, int num_jumps, double power, double power_reduction,
         int *total_healing, int *best_healing, Node **best_path, int *best_path_length,
         Node **current_path, int *best_heals, int *current_heals) {
    if (hop > num_jumps) return; // Base case
    node->visited = 1;

    // Calculate this hop's healing
    int healing = (int)rint(power * pow(1 - power_reduction, hop - 1));
    if (node->cur_PP + healing > node->max_PP) {
        healing = node->max_PP - node->cur_PP;
    }

    // Apply healing
    node->cur_PP += healing;
    *total_healing += healing;
    
    // Update current path info
    current_path[hop - 1] = node;
    current_heals[hop - 1] = healing;

    // Best path check
    if (*total_healing > *best_healing) {
        *best_healing = *total_healing;
        *best_path_length = hop;
        for (int i = 0; i < hop; i++) {
            best_path[i] = current_path[i];
            best_heals[i] = current_heals[i];
        }
    }

    // Recursion
    for (int i = 0; i < node->adj_size; i++) {
        if (!node->adj[i]->visited) {
            dfs(node->adj[i], hop + 1, num_jumps, power, power_reduction, 
                total_healing, best_healing,
                best_path, best_path_length, current_path,
                best_heals, current_heals);
        }
    }

    // Backtrack
    node->visited = 0;
    node->cur_PP -= healing;
    *total_healing -= healing;
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s initial_range jump_range num_jumps initial_power power_reduction < input_file\n", argv[0]);
        return 1;
    }

    // Parse command line arguments
    int initial_range = atoi(argv[1]);
    int jump_range = atoi(argv[2]);
    int num_jumps = atoi(argv[3]);
    int initial_power = atoi(argv[4]);
    double power_reduction = atof(argv[5]);

    // Read input and create graph
    nodes = (Node **)malloc(sizeof(Node *) * 1000);
    read_input();
    create_graph(jump_range);

    // Find Urgosa
    Node *urgosa = NULL;
    for (int i = 0; i < num_nodes; i++) {
        if (strcmp(nodes[i]->name, "Urgosa_the_Healing_Shaman") == 0) {
            urgosa = nodes[i];
            break;
        }
    }
    
    // Initialize variables for DFS
    int best_healing = 0;
    int total_healing = 0;
    Node **best_path = (Node **)malloc(sizeof(Node *) * (num_jumps + 1));
    Node **current_path = malloc(sizeof(Node *) * (num_jumps + 1));
    int *best_heals = malloc(sizeof(int) * (num_jumps + 1));
    int *current_heals = malloc(sizeof(int) * (num_jumps + 1));
    int best_path_length = 0;

    // Start DFS from Urgosa
    for (int i = 0; i < num_nodes; i++) {
        if (distance(urgosa, nodes[i]) <= initial_range) {
            dfs(nodes[i], 1, num_jumps, initial_power, power_reduction,
                &total_healing, &best_healing,
                best_path, &best_path_length, current_path,
                best_heals, current_heals);
        }
    }

    // Output results
    for (int i = 0; i < best_path_length; i++) {
        printf("%s %d\n", best_path[i]->name, best_heals[i]);
    }
    printf("Total_Healing %d\n", best_healing);

    // Free allocated memory
    for (int i = 0; i < num_nodes; i++) {
        free(nodes[i]);
    }
    free(nodes);

    return 0;
}