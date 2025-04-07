#include "jrb.h"
#include "dllist.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct person {
    char *name;
    char sex; // 'M' for male, 'F' for female, 'U' for unknown
    struct person *father;
    struct person *mother;
    Dllist children;
    int visited;
    int unprinted_parents;
} Person;

JRB people;
Person *last_person = NULL;

// Create a new person and return a pointer to it
Person *create_person(char *name) {
    Person *p = (Person *)malloc(sizeof(Person));
    p->name = strdup(name);
    p->sex = 'U';
    p->father = NULL;
    p->mother = NULL;
    p->children = new_dllist();
    p->visited = 0;
    p->unprinted_parents = 0;
    return p;
}

// Get a person by name, creating them if they don't exist
Person *get_person(char *name) {
    JRB node = jrb_find_str(people, name);
    if (node) {
        return (Person *)node->val.v;
    } else {
        Person *p = create_person(name);
        jrb_insert_str(people, strdup(name), new_jval_v(p));
        return p;
    }
}

// Add a child to a parent
void add_child(Person *parent, Person *child) {
    dll_append(parent->children, new_jval_v(child));
}

// Read input from stdin and process commands
void read_input() {
    char line[256]; // Buffer for each line of input
    int line_number = 0; // Line number for error reporting
    while (fgets(line, sizeof(line), stdin)) {
        line_number++;
        char *command = strtok(line, " ");
        if (!command) {
            continue; // Skip empty lines or lines with only whitespace
        } 

        // Process commands
        if (strcmp(command, "PERSON") == 0) {
            char *name = strtok(NULL, "\n");
            if (name) {
               last_person = get_person(name);
            }
        } else if (strcmp(command, "SEX") == 0) {
            // Try to parse two tokens
            char *token1 = strtok(NULL, " ");
            char *token2 = strtok(NULL, "\n");
        
            if (token1 == NULL) {
                // No tokens after SEX = do nothing or report an error
            }
            else if (token2 == NULL) {
                // One token after SEX, assume it's F or M
                if (last_person) {
                    if (last_person->sex != 'U' && last_person->sex != token1[0]) {
                        fprintf(stderr, "Bad input - sex mismatch on line %d\n", line_number);
                        exit(1);
                    } else {
                        last_person->sex = token1[0];
                    }
                }
            }
            else {
                // Two tokens -> token1 is the name, token2 is the sex
                Person *p = get_person(token1);
                if (p->sex != 'U' && p->sex != token2[0]) {
                    fprintf(stderr, "Bad input - sex mismatch on line %d\n", line_number);
                    exit(1);
                } else {
                    p->sex = token2[0];
                }
            }
        } else if (strcmp(command, "FATHER") == 0) {
            char *name = strtok(NULL, " ");
            char *father_name = strtok(NULL, "\n");
            if (name != NULL && father_name != NULL) {
                Person *p = get_person(name);
                Person *father = get_person(father_name);
                if (p->father && strcmp(p->father->name, father_name) != 0) {
                    // Conflicting father information, do nothing (for now)
                } else {
                    p->father = father;
                    add_child(father, p);
                }
            }
        } else if (strcmp(command, "MOTHER") == 0) {
            char *name = strtok(NULL, " ");
            char *mother_name = strtok(NULL, "\n");
            if (name != NULL && mother_name != NULL) {
                Person *p = get_person(name);
                Person *mother = get_person(mother_name);
                if (p->mother && strcmp(p->mother->name, mother_name) != 0) {
                    // Conflicting mother information, do nothing (for now)
                } else {
                    p->mother = mother;
                    add_child(mother, p);
                }
            }
        } else if (strcmp(command, "FATHER_OF") == 0) {
            char *father_name = strtok(NULL, " ");
            char *child_name = strtok(NULL, "\n");
            if (father_name != NULL && child_name != NULL) {
                Person *father = get_person(father_name);
                Person *child = get_person(child_name);
                if (father->sex == 'F') {
                    // Conflicting father information, do nothing (for now)
                } else {
                    father->sex = 'M';
                    if (child->father && strcmp(child->father->name, father_name) != 0) {
                        // Conflicting father information, do nothing (for now)
                    } else {
                        child->father = father;
                        add_child(father, child);
                    }
                }
            }
        } else if (strcmp(command, "MOTHER_OF") == 0) {
            char *mother_name = strtok(NULL, " ");
            char *child_name = strtok(NULL, "\n");
            if (mother_name != NULL && child_name != NULL) {
                Person *mother = get_person(mother_name);
                Person *child = get_person(child_name);
                if (mother->sex == 'M') {
                    // Conflicting mother information, do nothing (for now)
                } else {
                    mother->sex = 'F';
                    if (child->mother && strcmp(child->mother->name, mother_name) != 0) {
                        // Conflicting mother information, do nothing (for now)
                    } else {
                        child->mother = mother;
                        add_child(mother, child);
                    }
                }
            }
        }
    }
}

// Detect cycles in the family tree
void detect_cycles(Person *p) {
    if (p->visited == 1) { // If the node has already been visited once, there is a cycle
        fprintf(stderr, "Bad input -- cycle in specification\n");
        exit(1);
    }

    // If the node has not been visited yet, mark it as visited and continue DFS
    if (p->visited == 0) {
        p->visited = 1;
        if (p->father) detect_cycles(p->father);
        if (p->mother) detect_cycles(p->mother);
        Dllist child;
        dll_traverse(child, p->children) {
            detect_cycles((Person *)child->val.v);
        }
        p->visited = 2;
    }
}

// Print the family tree for a person
void print_person(Person *p) {
    printf("%s\n", p->name);
    printf("  Sex: %s\n", p->sex == 'M' ? "Male" : p->sex == 'F' ? "Female" : "Unknown");
    printf("  Father: %s\n", p->father ? p->father->name : "Unknown");
    printf("  Mother: %s\n", p->mother ? p->mother->name : "Unknown");
    printf("  Children: ");
    if (dll_empty(p->children)) {
        printf("None\n");
    } else {
        printf("\n");
        Dllist child;
         dll_traverse(child, p->children) {
            Person *c = (Person *)child->val.v;
            printf("    %s\n", c->name);
        }
    }
    printf("\n");
}

// Print the family tree in a topological order
void print_graph() {
    // Create a queue (dllist) to hold ready-to-print people
    Dllist wq = new_dllist();

    // Initialize the queue with all people who have no unprinted parents
    JRB node;
    jrb_traverse(node, people) {
        Person *p = (Person *)node->val.v;
        // If this person has zero unprinted parents, put them into the queue
        if (p->unprinted_parents == 0) {
            dll_append(wq, new_jval_v(p));
        }
    }

    // Process the queue
    while (!dll_empty(wq)) {
        Dllist first = dll_first(wq);
        Person *p = (Person *)first->val.v;
        dll_delete_node(first);

        // Print this person
        print_person(p);

        // Decrement unprinted_parents for each child
        Dllist child;
        dll_traverse(child, p->children) {
            Person *c = (Person *)child->val.v;
            c->unprinted_parents--;
            // If a child’s unprinted_parents is now 0, add it to the queue
            if (c->unprinted_parents == 0) {
                dll_append(wq, new_jval_v(c));
            }
        }
    }

    free_dllist(wq);
}

int main(int argc, char *argv[]) {
    people = make_jrb();
    read_input();
    JRB node;
    jrb_traverse(node, people) {
        Person *p = (Person *)node->val.v;
        if (p->father) p->unprinted_parents++;
        if (p->mother) p->unprinted_parents++;
    }
    jrb_traverse(node, people) {
        Person *p = (Person *)node->val.v;
        if (p->visited == 0) {
            detect_cycles(p);
        }
    }
    jrb_traverse(node, people) {
        Person *p = (Person *)node->val.v;
        p->visited = 0;
    }
    print_graph();
    return 0;
}