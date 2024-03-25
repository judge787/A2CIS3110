#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h> 


#define DICT_SIZE 150000
#define MAX_MISPELLED_WORDS 3
#define MAX_LENGTH 256
#define OUTPUT_FILENAME "judgej_A2.out"

pthread_mutex_t fileMutex;
pthread_mutex_t summaryMutex;
pthread_cond_t allThreadsDone = PTHREAD_COND_INITIALIZER;  // Condition variable to signal all threads are done
int activeThreads = 0;
int saveToFile = 0;

typedef struct {
    char word[MAX_LENGTH];
    int count;
} WordCounter;

typedef struct {
    char textFile[MAX_LENGTH];
    char dictFile[MAX_LENGTH];
} TaskData;

typedef struct DictItem {
    char word[MAX_LENGTH];
    struct DictItem* next;
} DictItem;

typedef struct {
    int totalFilesProcessed;
    int totalSpellingErrors;
    WordCounter commonMisspellings[MAX_MISPELLED_WORDS];
} SummaryData;

DictItem* dictHashTable[DICT_SIZE] = {NULL};
SummaryData summary = {0, 0, {{"", 0}, {"", 0}, {"", 0}}};

unsigned long hashWord(const char* word) {
    unsigned long hash = 5381;
    int c;
    while ((c = *word++))
        hash = ((hash << 5) + hash) + c;
    return hash % DICT_SIZE;
}

void toLowerCase(char *str) {
    if (str) {
        for (int i = 0; str[i]; i++) {
            str[i] = tolower((unsigned char)str[i]);
        }
    }
}

void loadDictionary(const char* dictFile) {
    FILE* file = fopen(dictFile, "r");
    if (!file) {
        perror("Failed to open dictionary file");
        exit(1);
    }
    char word[MAX_LENGTH];
    while (fscanf(file, "%255s", word) != EOF) {
        toLowerCase(word);
        unsigned long index = hashWord(word);
        DictItem* newItem = malloc(sizeof(DictItem));
        strcpy(newItem->word, word);
        newItem->next = dictHashTable[index];
        dictHashTable[index] = newItem;
    }
    fclose(file);
}



int isWordInDictionary(const char* word) {
    unsigned long index = hashWord(word);
    for (DictItem* item = dictHashTable[index]; item != NULL; item = item->next) {
        if (strcmp(item->word, word) == 0)
            return 1;
    }
    return 0;
}



void updateSummary(const char* misspelledWord) {
    pthread_mutex_lock(&summaryMutex);

    // Update the total number of spelling errors
    if (misspelledWord != NULL) {
        summary.totalSpellingErrors++;

        // Check if the misspelled word is already in the common misspellings
        int found = 0;
        for (int i = 0; i < MAX_MISPELLED_WORDS && !found; i++) {
            if (strcmp(summary.commonMisspellings[i].word, misspelledWord) == 0) {
                summary.commonMisspellings[i].count++;
                found = 1;
            }
        }

        // If the word is not in the common misspellings, add or replace it
        if (!found) {
            int minIndex = -1, minCount = INT_MAX;
            for (int i = 0; i < MAX_MISPELLED_WORDS; i++) {
                if (summary.commonMisspellings[i].count < minCount) {
                    minCount = summary.commonMisspellings[i].count;
                    minIndex = i;
                }
            }
            if (minIndex != -1) {
                strcpy(summary.commonMisspellings[minIndex].word, misspelledWord);
                summary.commonMisspellings[minIndex].count = 1;
            }
        }

        // Sort common misspellings by frequency
        for (int i = 0; i < MAX_MISPELLED_WORDS - 1; i++) {
            for (int j = i + 1; j < MAX_MISPELLED_WORDS; j++) {
                if (summary.commonMisspellings[i].count < summary.commonMisspellings[j].count) {
                    WordCounter temp = summary.commonMisspellings[i];
                    summary.commonMisspellings[i] = summary.commonMisspellings[j];
                    summary.commonMisspellings[j] = temp;
                }
            }
        }
    } 

    pthread_mutex_unlock(&summaryMutex);
}


char* removePunctuationAndToLower(const char* word) {
    size_t len = strlen(word);
    char* sanitizedWord = (char*)malloc((len + 1) * sizeof(char));
    int index = 0;
    for (size_t i = 0; i < len; i++) {
        // Copy only alphanumeric characters and apostrophes
        if (isalnum((unsigned char)word[i]) || word[i] == '\'') {
            sanitizedWord[index++] = tolower((unsigned char)word[i]);
        }
    }
    sanitizedWord[index] = '\0'; // Null-terminate the sanitized string
    return sanitizedWord;
}


void saveResultsToFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        perror("Failed to open file for writing summary");
        return;
    }
    fprintf(file, "Number of files processed: %d\n", summary.totalFilesProcessed);
    fprintf(file, "Number of spelling errors: %d\n", summary.totalSpellingErrors);
    for (int i = 0; i < MAX_MISPELLED_WORDS; i++) {
        fprintf(file, "%s: %d times\n", summary.commonMisspellings[i].word, summary.commonMisspellings[i].count);
    }
    fclose(file);
}

void printSummary() {
    printf("Number of files processed: %d\n", summary.totalFilesProcessed);
    printf("Number of spelling errors: %d\n", summary.totalSpellingErrors);
    for (int i = 0; i < MAX_MISPELLED_WORDS; i++) {
        printf("%s: %d times\n", summary.commonMisspellings[i].word, summary.commonMisspellings[i].count);
    }
}

char* removePunctuation(const char* word) {
    size_t len = strlen(word);
    char* sanitizedWord = malloc(len + 1); // +1 for the null terminator
    int index = 0;

    for (size_t i = 0; i < len; i++) {
        if (!ispunct(word[i]) || word[i] == '-') {
            sanitizedWord[index++] = word[i];
        }
    }
    sanitizedWord[index] = '\0'; // Null-terminate the sanitized string
    return sanitizedWord;
}

void* spellCheckTask(void* arg) {
    TaskData* data = (TaskData*)arg;

    // Lock the summary mutex and increment the number of files processed
    pthread_mutex_lock(&summaryMutex);
    summary.totalFilesProcessed++;
    pthread_mutex_unlock(&summaryMutex);

    // Open the text file for spell checking
    FILE* textFile = fopen(data->textFile, "r");
    if (!textFile) {
        perror("Failed to open text file");
        pthread_exit(NULL);
    }

    char word[MAX_LENGTH];
    // Read words from the text file one by one
    while (fscanf(textFile, "%255s", word) == 1) {
        toLowerCase(word);
        char* sanitizedWord = removePunctuation(word); // Remove punctuation from the word
        if (!isWordInDictionary(sanitizedWord)) { // Check if the word is in the dictionary
            updateSummary(sanitizedWord); // Update the summary with the misspelled word
        }
        free(sanitizedWord); // Free the memory allocated for the sanitized word
    }

    fclose(textFile); // Close the text file

    // Lock the summary mutex to decrement the active thread count
    pthread_mutex_lock(&summaryMutex);
    activeThreads--;
    // If there are no active threads left, signal the condition variable
    if (activeThreads == 0) {
        pthread_cond_signal(&allThreadsDone);
    }
    pthread_mutex_unlock(&summaryMutex);

    free(data); // Free the task data allocated memory
    pthread_exit(NULL); // Exit the thread
}



void startSpellCheck(const char* dictFile) {
    TaskData* taskData = malloc(sizeof(TaskData));
    strcpy(taskData->dictFile, dictFile);
    printf("Enter text file name: ");
    scanf("%s", taskData->textFile);

    pthread_mutex_lock(&summaryMutex);
    activeThreads++;
    pthread_mutex_unlock(&summaryMutex);

    pthread_t threadId;
    pthread_create(&threadId, NULL, spellCheckTask, (void*)taskData);
    pthread_detach(threadId);
}

void waitAllThreadsComplete() {
    pthread_mutex_lock(&summaryMutex);
    while (activeThreads > 0) {
        pthread_cond_wait(&allThreadsDone, &summaryMutex);
    }
    pthread_mutex_unlock(&summaryMutex);
}

int main(int argc, char *argv[]) {
    // Check if the -l argument was provided for logging to a file
    if (argc > 1 && strcmp(argv[1], "-l") == 0) {
        saveToFile = 1;
    }

    // Initialize mutexes for file and summary operations
    pthread_mutex_init(&fileMutex, NULL);
    pthread_mutex_init(&summaryMutex, NULL);

    // Load dictionary file specified by user
    char dictFile[MAX_LENGTH];
    printf("Enter dictionary file name: ");
    scanf("%255s", dictFile);
    loadDictionary(dictFile);

    // Start user interface for spell-checking tasks
    int choice;
    do {
        printf("\n1. Start a new spellchecking task\n2. Exit\nSelect an option: ");
        scanf("%d", &choice);

        if (choice == 1) {
            startSpellCheck(dictFile);
        }
    } while (choice != 2);

    // Wait for all active spell-checking threads to complete
    waitAllThreadsComplete();

    // Save summary to a file or print to the console depending on user's choice
    if (saveToFile) {
        saveResultsToFile(OUTPUT_FILENAME);
    } else {
        printSummary();
    }

    // Clean up allocated memory and destroy mutexes and condition variables
    for (int i = 0; i < DICT_SIZE; i++) {
        DictItem* item = dictHashTable[i];
        while (item != NULL) {
            DictItem* temp = item;
            item = item->next;
            free(temp);
        }
    }

    pthread_mutex_destroy(&fileMutex);
    pthread_mutex_destroy(&summaryMutex);

    return 0;
}
