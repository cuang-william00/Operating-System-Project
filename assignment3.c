#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdatomic.h>

struct Node{
    char* line;
    struct Node* next;
    struct Node* book_next;
    struct Book* book;  // For Part 2
};

struct ThreadArgs {
    int client_sock;
    int book_number;
};

struct AnalysisArgs {
    char* pattern;
};

struct Book {
    char *title;
    struct Node *head;
    struct Book *next;
    int number;
    int count;
};

struct Book *book_list = NULL;  // Head of the book list
struct Node *head = NULL;  // Head of the shared list
struct Node **analysisNode = &head; // The node to consume for analysis

pthread_mutex_t analysis_consume_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sorted_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex for list access
pthread_cond_t cond_analysisCreated = PTHREAD_COND_INITIALIZER;

pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;

struct timespec printinterval;
int printintervalseconds = 5;

void print_shared_list() {
    struct Node *current = head;
    printf("Shared List:\n");
    while (current != NULL) {
        printf("%s", current->line);
        current = current->next;
    }
    printf("\n");
}

void print_book(struct Book *book, int book_number) {
    char filename[20];
    snprintf(filename, sizeof(filename), "book_%02d.txt", book_number);

    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Could not open file for writing");
        return;
    }

    struct Node *current = book->head;
    while (current != NULL) {
        fprintf(file, "%s", current->line);
        current = current->book_next;
    }

    fclose(file);
}

void free_nodes(struct Node *head) {
    struct Node *tmp;

    while (head != NULL) {
       tmp = head;
       head = head->next;
       free(tmp->line);
       free(tmp);
    }
}

void free_books(struct Book *head) {
    struct Book *tmp;

    while (head != NULL) {
        tmp = head;
        head = head->next;
        free(tmp->title);
        free(tmp);
    }
}

void error(const char *msg) {
    perror(msg);
    exit(1);
}


void set_non_blocking(int sock) {
    int opts;

    opts = fcntl(sock, F_GETFL);
    if (opts < 0) {
        perror("fcntl(F_GETFL)");
        exit(EXIT_FAILURE);
    }

    opts = (opts | O_NONBLOCK);
    if (fcntl(sock, F_SETFL, opts) < 0) {
        perror("fcntl(F_SETFL)");
        exit(EXIT_FAILURE);
    }
}

char *extractFirstLine(const char *input) {
    const char *newline = strchr(input, '\n'); // Find the first newline character
    if (newline) {
        size_t length = newline - input;
        char *firstLine = (char *)malloc(length + 1);
        if (firstLine) {
            strncpy(firstLine, input, length);
            firstLine[length] = '\0'; // Null-terminate the new string
            return firstLine;
        }
    }
    return NULL; // Return NULL if no newline character is found
}

void *client_handler(void *arg) {
    struct ThreadArgs *args = (struct ThreadArgs *)arg;
    int client_sock = args->client_sock;
    int book_number = args->book_number;
    char buffer[256];
    struct Node *new_node;
    fd_set read_fds;
    struct timeval timeout;
    int isFirstLine = 1;
    struct Book *currentBook = NULL;

    while(1){
        FD_ZERO(&read_fds);
        FD_SET(client_sock, &read_fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(client_sock + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0 && errno != EINTR) {
            perror("select error");
            break;
        }

        if(FD_ISSET(client_sock, &read_fds)){
            ssize_t bytes_read = read(client_sock, buffer, sizeof(buffer) - 1);
            if(bytes_read < 0) {
                if(errno == EWOULDBLOCK || errno == EAGAIN){
                    // No more data to read
                    break;
                }
                perror("ERROR reading from socket");
                break;
            } else if(bytes_read == 0){
                //client has closed the connection or sent an empty file
                break;
            }

            buffer[bytes_read] = '\0';

            pthread_mutex_lock(&list_mutex);

            new_node = malloc(sizeof(struct Node));
            if (isFirstLine) {
                isFirstLine = 0;
                struct Book *new_book = malloc(sizeof(struct Book));
                new_book->title = extractFirstLine(buffer);
                new_book->head = new_node;
                new_book->next = NULL;
                new_book->number = book_number;
                new_book->count = 0;

                pthread_mutex_lock(&sorted_list_mutex);
                if(book_list == NULL){
                    book_list = new_book;
                }
                else{
                    struct Book* temp = book_list;
                    while(temp->next != NULL) temp = temp->next;
                    temp->next = new_book;
                }

                pthread_mutex_unlock(&sorted_list_mutex);

                currentBook = new_book;
            } else {
                struct Node *book_temp = currentBook->head;
                while (book_temp->book_next != NULL) {
                    book_temp = book_temp->book_next;
                }
                book_temp->book_next = new_node;
            }

            new_node->line = strdup(buffer);
            new_node->next = NULL;
            new_node->book_next = NULL;
            new_node->book = currentBook;

            if(head == NULL){
                head = new_node;
            }else{
                struct Node *temp = head;
                while(temp->next != NULL){ 
                    temp = temp->next;
                }
                temp->next = new_node; 
            }

            pthread_mutex_unlock(&list_mutex);
            pthread_cond_broadcast(&cond_analysisCreated);
            pthread_mutex_lock(&stdout_mutex);
            printf("Added node to book number %d\n", book_number);
            pthread_mutex_unlock(&stdout_mutex);
        }
    }

    if (currentBook != NULL) {  // Here
        print_book(currentBook, book_number);
    }
    close(client_sock);
    free(arg);
    return NULL;
}

void printSortedList(){
    pthread_mutex_lock(&sorted_list_mutex);
    struct Book* temp = book_list;
    printf("\n");
    printf("========================================================================\n");
    printf("\t Books in descending order of pattern count\n");
    printf("========================================================================\n");

    while(temp != NULL){
        printf("%s - %d\n", temp->title, temp->count);
        temp = temp->next;
    }
    pthread_mutex_unlock(&sorted_list_mutex);
    printf("\n");
}

// Waits until there is a node to process, or its time to print
struct Node* consumeNode(){
        static atomic_int printCycleID = ATOMIC_VAR_INIT(0);
        pthread_mutex_lock(&analysis_consume_mutex);

        while(1){
            // Wait for condition variable, or for the print timeout
            int waitRes = 0;
            while(*analysisNode == NULL){
                int waitID = atomic_load(&printCycleID);
                waitRes = pthread_cond_timedwait(&cond_analysisCreated, &analysis_consume_mutex, &printinterval);

                if(waitRes == ETIMEDOUT && atomic_fetch_add(&printCycleID, 1) == waitID){
                    // Try to get the stdout mutex, if its already locked, go back to waiting
                    if(pthread_mutex_trylock(&stdout_mutex) == 0){
                        pthread_mutex_unlock(&analysis_consume_mutex);
                        printSortedList();
                        clock_gettime(CLOCK_REALTIME, &printinterval);
                        printinterval.tv_sec += printintervalseconds;
                        pthread_mutex_unlock(&stdout_mutex);
                        pthread_mutex_lock(&analysis_consume_mutex);
                    }
                } 
            }

            if(*analysisNode != NULL){
                // Make sure this print cycle hasn't been done already
                // Consume
                struct Node* targetNode = *analysisNode;
                analysisNode = &targetNode->next;

                pthread_mutex_unlock(&analysis_consume_mutex);

                return targetNode;
            }         
        }

}

int countSubstringOccurrences(const char *haystack, const char *needle) {
    int count = 0;
    int needleLength = strlen(needle);
    
    while (haystack) {
        haystack = strstr(haystack, needle);
        if (haystack != NULL) {
            count++;
            haystack += needleLength;
        }
    }
    return count;
}

int analyze(struct Node* targetNode, const char* pattern){
    int count = countSubstringOccurrences(targetNode->line, pattern);

    return count;
}

// Sorts target node inside the sortedBookList by count, returns new head
struct Book* bubbleSort(struct Book* head, struct Book* target, struct Book* prev){
    if(head->next != target) {
        bubbleSort(head->next, target, head);
        target = head->next;
    }

    // If target became bigger, swap the two
    if(target->count > head->count) { 
        if(prev != NULL) prev->next = target;
        head->next = target->next;
        target->next = head;
        return target;
    }
    return head;
}

void analysis(void *arg){
    struct AnalysisArgs *args = (struct AnalysisArgs*) arg;
    char* pattern = args->pattern;
    if(pattern == NULL) return;

    while(1){
        struct Node* targetNode = consumeNode();

        // Analyze
        int result = analyze(targetNode, pattern);
        if(result){
            // Update the list order
            pthread_mutex_lock(&sorted_list_mutex);
            targetNode->book->count += result;
            // Sort if its out of place
            if(book_list != targetNode->book)
                book_list = bubbleSort(book_list, targetNode->book, NULL);
            pthread_mutex_unlock(&sorted_list_mutex);
        }
    }

    free(arg);
}


int main(int argc, char *argv[]){
    int server_sock, port;
    char *search_pattern = NULL;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread;
    int opt;

    int book_number = 0;

    // Command line parsing
    while ((opt = getopt(argc, argv, "l:p:")) != -1) {
        switch (opt) {
            case 'l':
                port = atoi(optarg);
                break;
            case 'p':
                search_pattern = optarg;
                break;
            case 'i':
                printintervalseconds = atoi(optarg);
            default:
                fprintf(stderr, "Usage: %s -l <port> [-p <search_pattern>] [-i <interval>]\n", argv[0]);
                exit(1);
        }
    }

    if (port == 0) {
        fprintf(stderr, "Usage: %s -l <port> [-p <search_pattern>] [-i <interval>]\n", argv[0]);
        exit(1);
    }

    for(int i = 0; i < 3; i++){
        struct AnalysisArgs* aa = malloc(sizeof(struct AnalysisArgs));
        aa->pattern = search_pattern;
        pthread_create(&thread, NULL, analysis, aa);
        pthread_detach(&thread);
    }

    server_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (server_sock < 0)
        error("ERROR opening socket");

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        error("ERROR on binding");

    listen(server_sock, 5);

    printf("Starting server\n");
    while (1){

        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);

        if (client_sock < 0)
            error("ERROR on accept");

        set_non_blocking(client_sock);

        book_number++;  // Increment the book_number

        struct ThreadArgs *args = malloc(sizeof(struct ThreadArgs));
        args->client_sock = client_sock;
        args->book_number = book_number;

        if (pthread_create(&thread, NULL, client_handler, args) < 0)
            error("ERROR creating thread");

        pthread_detach(thread);
    }

    free_nodes(head);
    free_books(book_list);
    close(server_sock);
    return 0;
}
