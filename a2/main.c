// // Include necessary header files
// #include <stdio.h>    // Standard I/O operations
// #include <stdlib.h>   // Standard library for memory allocation, exit, etc.
// #include <string.h>   // String operations like strcpy
// #include <pthread.h>  // POSIX threads for threading functionality
// #include <unistd.h>   // POSIX API for system calls

// // Declare a mutex globally for synchronizing access to shared resources
// pthread_mutex_t fileMutex;

// // Define a structure to hold task data (file names)
// typedef struct {
//     char textFile[256];  // Name of the text file to spell-check
//     char dictFile[256];  // Name of the dictionary file
// } TaskData;

// // Function executed by each spell-checking thread
// void *spellCheckTask(void *arg) {
//     // Cast the void pointer back to the correct type
//     TaskData *data = (TaskData *)arg;

//     // Print the task starting message
//     printf("Starting spell-check for %s using dictionary %s\n", data->textFile, data->dictFile);

//     // Simulate doing some work, e.g., spell-checking
//     sleep(1);

//     // Lock the mutex before accessing shared resources
//     pthread_mutex_lock(&fileMutex);
//     // Simulate saving results to a file
//     printf("Finished spell-check for %s, results saved.\n", data->textFile);
//     // Unlock the mutex after done with shared resources
//     pthread_mutex_unlock(&fileMutex);

//     // Free the dynamically allocated memory for task data
//     free(data);
//     // Terminate the thread cleanly
//     pthread_exit(NULL);
// }

// // Function to start a new spell-checking task
// void startSpellCheck() {
//     // Temporary buffers to hold user input
//     char textFile[256], dictFile[256];
//     // Prompt the user for the text file name
//     printf("Enter text file name: ");
//     scanf("%s", textFile);
//     // Prompt the user for the dictionary file name
//     printf("Enter dictionary file name: ");
//     scanf("%s", dictFile);

//     // Allocate memory for the task data
//     TaskData *taskData = malloc(sizeof(TaskD5ata));
//     // Copy user input into the task data structure
//     strcpy(taskData->textFile, textFile);
//     strcpy(taskData->dictFile, dictFile);

//     // Declare a thread ID variable
//     pthread_t threadId;
//     // Create a new thread to run the spell-check task
//     pthread_create(&threadId, NULL, spellCheckTask, (void *)taskData);
//     // Detach the thread so it cleans up after itself upon completion
//     pthread_detach(threadId);
// }

// // Main function - entry point of the program
// int main(int argc, char *argv[]) {
//     // Choice variable for user input in the menu
//     int choice;
//     // Initialize the mutex before using it
//     pthread_mutex_init(&fileMutex, NULL);

//     // Main loop for the menu
//     do {
//         // Display menu options to the user
//         printf("1. Start a new spellchecking task\n");
//         printf("2. Exit\n");
//         printf("Select an option: ");
//         scanf("%d", &choice);

//         // Handle user's choice
//         switch (choice) {
//             case 1:  // Start a new spellchecking task
//                 startSpellCheck();
//                 break;
//             case 2:  // Exit the program
//                 printf("Exiting...\n");
//                 break;
//             default:  // Invalid option
//                 printf("Invalid option.\n");
//         }
//     } while (choice != 2);  // Continue until the user chooses to exit

//     // Clean up: Destroy the mutex before exiting the program
//     pthread_mutex_destroy(&fileMutex);
//     return 0;  // Return success
// }

#include <stdio.h>    // Include standard input and output library
#include <stdlib.h>   // Include standard library for malloc, exit
#include <string.h>   // Include string library for string manipulation functions
#include <pthread.h>  // Include POSIX threads library for thread functions
#include <unistd.h>   // Include POSIX standard library for POSIX OS API

#define DICT_SIZE 150000 // Define the size of the dictionary hash table

pthread_mutex_t fileMutex; // Declare a mutex for file writing synchronization

// Define a struct to hold the filenames for spell checking
typedef struct {
    char textFile[256];  // Array to hold the name of the text file
    char dictFile[256];  // Array to hold the name of the dictionary file
} TaskData;

// Define a struct for the dictionary items in the hash table
typedef struct DictItem {
    char word[256];           // Array to hold a word from the dictionary
    struct DictItem* next;    // Pointer to the next item in case of collision (linked list)
} DictItem;

DictItem* dictHashTable[DICT_SIZE] = {NULL}; // Initialize the hash table with NULLs

// Define a hash function for words
unsigned long hashWord(const char* word) {
    unsigned long hash = 5381; // Initialize hash value
    int c;
    while ((c = *word++)) // Iterate through each character in the word
        hash = ((hash << 5) + hash) + c; // Calculate hash value
    return hash % DICT_SIZE; // Return the hash value modulo DICT_SIZE
}

// Function to load the dictionary into the hash table
void loadDictionary(const char* dictFile) {
    FILE* file = fopen(dictFile, "r"); // Open the dictionary file for reading
    if (!file) {
        perror("Failed to open dictionary file"); // Print error if file can't be opened
        exit(1); // Exit the program with error status
    }

    char word[256]; // Array to hold words read from the file
    while (fscanf(file, "%255s", word) != EOF) { // Read words until end of file
        unsigned long index = hashWord(word); // Get hash index for the word
        DictItem* newItem = malloc(sizeof(DictItem)); // Allocate memory for a new dictionary item
        strcpy(newItem->word, word); // Copy the word into the new item
        newItem->next = dictHashTable[index]; // Insert the new item at the beginning of the list at the index
        dictHashTable[index] = newItem; // Update the head of the list at the index to the new item
    }
    fclose(file); // Close the dictionary file
}

// Function to check if a word is in the dictionary
int isWordInDictionary(const char* word) {
    unsigned long index = hashWord(word); // Get hash index for the word
    for (DictItem* item = dictHashTable[index]; item != NULL; item = item->next) { // Iterate through the list at the index
        if (strcmp(item->word, word) == 0) // Compare the word with the item's word
            return 1; // Return 1 if word is found
    }
    return 0; // Return 0 if word is not found
}

// Function executed by each spell-checking thread
void* spellCheckTask(void* arg) {
    TaskData* data = (TaskData*)arg; // Cast arg back to TaskData pointer
    printf("Starting spell-check for %s using dictionary %s\n", data->textFile, data->dictFile); // Print starting message

    FILE* textFile = fopen(data->textFile, "r"); // Open the text file for reading
    if (!textFile) {
        perror("Failed to open text file"); // Print error if file can't be opened
        pthread_exit(NULL); // Exit the thread
    }

    int errorCount = 0; // Initialize error count
    char word[256]; // Array to hold words read from the text file

    while (fscanf(textFile, "%255s", word) == 1) { // Read words until end of file
        if (!isWordInDictionary(word)) { // Check if word is not in dictionary
            errorCount++; // Increment error count
        }
    }
    fclose(textFile); // Close the text file

    pthread_mutex_lock(&fileMutex); // Lock the file mutex before writing to the file
    printf("%s %d misspelled words\n", data->textFile, errorCount); // Print the result
    pthread_mutex_unlock(&fileMutex); // Unlock the file mutex

    free(data); // Free the dynamically allocated TaskData
    pthread_exit(NULL); // Exit the thread
}

// Function to start a new spell-checking task
void startSpellCheck(const char* dictFile) {
    TaskData* taskData = malloc(sizeof(TaskData)); // Allocate memory for TaskData
    strcpy(taskData->dictFile, dictFile); // Copy the dictionary file name into TaskData
    printf("Enter text file name: "); // Prompt the user for the text file name
    scanf("%s", taskData->textFile); // Read the text file name

    pthread_t threadId; // Declare a variable to hold the thread ID
    pthread_create(&threadId, NULL, spellCheckTask, (void*)taskData); // Create a new thread for the spell-checking task
    pthread_detach(threadId); // Detach the thread
}

// Main function - entry point of the program
int main(int argc, char *argv[]) {
    pthread_mutex_init(&fileMutex, NULL); // Initialize the file mutex

    char dictFile[256]; // Array to hold the dictionary file name
    printf("Enter dictionary file name: "); // Prompt the user for the dictionary file name
    scanf("%s", dictFile); // Read the dictionary file name

    loadDictionary(dictFile); // Load the dictionary from the file

    int choice; // Variable to hold the user's menu choice
    do {
        printf("\n1. Start a new spellchecking task\n2. Exit\nSelect an option: "); // Print the menu
        scanf("%d", &choice); // Read the user's choice

        switch (choice) {
            case 1:
                startSpellCheck(dictFile); // Start a new spell-checking task
                break;
            case 2:
                printf("Exiting...\n"); // Exit the program
                break;
            default:
                printf("Invalid option.\n"); // Handle invalid menu option
        }
    } while (choice != 2); // Loop until the user chooses to exit

    pthread_mutex_destroy(&fileMutex); // Destroy the file mutex
    return 0; // Exit the program
}
