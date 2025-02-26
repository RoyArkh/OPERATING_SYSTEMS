#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

//struct to represent a block [hole or a process]
typedef struct Block {
    int start_address;
    int size;
    char process_id[10];  //hole for unused
    struct Block* next;
} Block;

Block* head = NULL;  //pointer to the first block
int total_memory = 0;

Block* createBlock(int start, int size, const char* pid) {
    Block* newBlock = (Block*)malloc(sizeof(Block));
    newBlock->start_address = start;
    newBlock->size = size;
    strcpy(newBlock->process_id, pid);
    newBlock->next = NULL;
    return newBlock;
}

void printError(char* error) {
    //error will be in red
    printf("\033[1;31m%s\033[0m\n", error);
}

void Allocate(char* PID, int size, char* type) {
    Block *current = head;
    Block *selected_hole = NULL;
    Block *prev = NULL;
    Block *selected_prev = NULL;
    
    while (current != NULL) {
        if (strcmp(current->process_id, "HOLE") == 0 && current->size >= size) {
            if (*type == 'F') {
                selected_hole = current;
                selected_prev = prev;
                break;
            }
            else if (*type == 'B') {
                if (selected_hole == NULL || current->size < selected_hole->size) {
                    selected_hole = current;
                    selected_prev = prev;
                }
            }
            else if (*type == 'W') {
                if (selected_hole == NULL || current->size > selected_hole->size) {
                    selected_hole = current;
                    selected_prev = prev;
                }
            }
        }
        prev = current;
        current = current->next;
    }
    
    if (selected_hole == NULL) {
        printError("ERROR: No hole large enough for allocation");
        return;
    }
    
    //hole is exactly the size
    if (selected_hole->size == size) {
        strcpy(selected_hole->process_id, PID);
    }
    //hole is larger, split it
    else {
        Block* new_process = createBlock(selected_hole->start_address, size, PID);
        Block* remaining_hole = createBlock(selected_hole->start_address + size, 
                                          selected_hole->size - size, "HOLE");
        
        if (selected_prev == NULL) {
            head = new_process;
        } else {
            selected_prev->next = new_process;
        }
        
        new_process->next = remaining_hole;
        remaining_hole->next = selected_hole->next;
        free(selected_hole);
    }
    
    printf("Successfully allocated %d bytes to process %s\n", size, PID);
}

void Deallocate(char* PID) {
    Block *current = head;
    Block *prev = NULL;
    
    //find process to deallocate
    while (current != NULL && strcmp(current->process_id, PID) != 0) {
        prev = current;
        current = current->next;
    }
    
    if (current == NULL) {
        printError("ERROR: Process not found");
        return;
    }
    
    //mark hole
    strcpy(current->process_id, "HOLE");
    
    //merge with next hole if adjacent
    if (current->next != NULL && strcmp(current->next->process_id, "HOLE") == 0) {
        Block* temp = current->next;
        current->size += temp->size;
        current->next = temp->next;
        free(temp);
    }
    
    //merge with previous hole if adjacent
    if (prev != NULL && strcmp(prev->process_id, "HOLE") == 0) {
        prev->size += current->size;
        prev->next = current->next;
        free(current);
    }
    
    printf("Successfully deallocated process %s\n", PID);
}

void Status() {
    Block* current = head;
    int total_allocated = 0;
    int total_free = 0;
    
    printf("\nMemory Status:\n");
    printf("-------------\n");
    
    while (current != NULL) {
        printf("Addresses [%d:%d] ", current->start_address, 
               current->start_address + current->size - 1);
        
        if (strcmp(current->process_id, "HOLE") == 0) {
            printf("Unused\n");
            total_free += current->size;
        } else {
            printf("Process %s\n", current->process_id);
            total_allocated += current->size;
        }
        current = current->next;
    }
    
    printf("\nTotal allocated memory: %d bytes\n", total_allocated);
    printf("Total free memory: %d bytes\n\n", total_free);
}


void Compact() {
    if (head == NULL || head->next == NULL) return;
    
    Block* current = head;
    int new_start = 0;
    
    //move all processes to the beginning
    while (current != NULL) {
        if (strcmp(current->process_id, "HOLE") != 0) {
            current->start_address = new_start;
            new_start += current->size;
        }
        current = current->next;
    }
    
    //combine all holes
    current = head;
    Block* new_head = NULL;
    Block* tail = NULL;
    int total_hole_size = 0;
    
    while (current != NULL) {
        if (strcmp(current->process_id, "HOLE") != 0) {
            Block* new_block = createBlock(current->start_address, 
                                         current->size, 
                                         current->process_id);
            if (new_head == NULL) {
                new_head = new_block;
                tail = new_block;
            } else {
                tail->next = new_block;
                tail = new_block;
            }
        } else {
            total_hole_size += current->size;
        }
        current = current->next;
    }
    
    if (total_hole_size > 0) {
        Block* final_hole = createBlock(new_start, total_hole_size, "HOLE");
        if (new_head == NULL) {
            new_head = final_hole;
        } else {
            tail->next = final_hole;
        }
    }
    
    //free old and update
    while (head != NULL) {
        Block* temp = head;
        head = head->next;
        free(temp);
    }
    head = new_head;
    
    printf("Memory compaction completed\n");
}




int main(int argc, char *argv[]) {
	
	/* TODO: fill the line below with your names and ids */
	printf(" Group Name: Name  \n Student(s) Name: Roya Arkhmammadova \n ID: 0081620 \n");
    
    // Initialize first hole
    if (argc == 2) {
        total_memory = atoi(argv[1]);
        head = createBlock(0, total_memory, "HOLE");
        printf("HOLE INITIALIZED AT ADDRESS %d WITH %d BYTES\n", 0, total_memory);
    } else {
        printError("ERROR Invalid number of arguments.");
        return 1;
    }
    
    while(1) {
        char input[100];
        printf("allocator>");
        fgets(input, 100, stdin);
        input[strcspn(input, "\n")] = 0;
        
        if(input[0] == '\0') continue;
        
        char* arguments[4];
        char* token = strtok(input, " ");
        int tokenCount = 0;
        
        while(token != NULL && tokenCount < 4) {
            arguments[tokenCount] = token;
            token = strtok(NULL, " ");
            tokenCount++;
        }
        
        //convert command to lowercase for case-insensitive comparison
        for(int i = 0; arguments[0][i]; i++) {
            arguments[0][i] = tolower(arguments[0][i]);
        }
        
        if(strcmp(arguments[0], "rq") == 0) {
            if(tokenCount == 4) {
                int size = atoi(arguments[2]);
                if(size > 0) {
                    Allocate(arguments[1], size, arguments[3]);
                } else {
                    printError("ERROR: Invalid size specified");
                }
            } else {
                printError("ERROR Expected expression: RQ \"PID\" \"Bytes\" \"Algorithm\"");
            }
        }
        else if(strcmp(arguments[0], "rl") == 0) {
            if(tokenCount == 2) {
                Deallocate(arguments[1]);
            } else {
                printError("ERROR Expected expression: RL \"PID\"");
            }
        }
        else if(strcmp(arguments[0], "status") == 0 || strcmp(arguments[0], "stat") == 0) {
            if(tokenCount == 1) {
                Status();
            } else {
                printError("ERROR Expected expression: STATUS");
            }
        }
        else if(strcmp(arguments[0], "c") == 0) {
            if(tokenCount == 1) {
                Compact();
            } else {
                printError("ERROR Expected expression: C");
            }
        }
        else if(strcmp(arguments[0], "exit") == 0) {
            if(tokenCount == 1) {
                printf("Exiting program.\n");
                exit(0);
            } else {
                printError("ERROR Expected expression: EXIT");
            }
        }
        else {
            printError("ERROR Invalid command");
        }
    }
    
    return 0;
}

