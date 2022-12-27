#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h> 

#include "pzip.h"

pthread_barrier_t barrier;

char* input_chars_local;
int char_segment_size;
int* char_frequency_local;
struct zipped_char* zipped_char_local;
int* zipped_chars_count_local;
// numElementsToWrite[i] represents the number of elements that thread i should
// write into the zipped_chars array.
size_t* numElementsToWrite;
// Index of starting point to write data to in zipped_chars
// for current thread.
size_t startOutputIndex;

pthread_mutex_t a_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * pzip() - zip an array of characters in parallel
 *
 * Inputs:
 * @n_threads:		   The number of threads to use in pzip
 * @input_chars:		   The input characters (a-z) to be zipped
 * @input_chars_size:	   The number of characaters in the input file
 *
 * Outputs:
 * @zipped_chars:       The array of zipped_char structs
 * @zipped_chars_count:   The total count of inserted elements into the zippedChars array.
 * @char_frequency[26]: Total number of occurences
 *
 * NOTE: All outputs are already allocated. DO NOT MALLOC or REASSIGN THEM !!!
 *
 */
void pzip(int n_threads, char* input_chars, int input_chars_size,
    struct zipped_char* zipped_chars, int* zipped_chars_count,
    int* char_frequency)
{

    // Initialize globals to be shared by threads
    input_chars_local = input_chars;
    char_segment_size = input_chars_size / n_threads;
    char_frequency_local = char_frequency;
    zipped_char_local = zipped_chars;
    startOutputIndex = 0;
    zipped_chars_count_local = zipped_chars_count;
    numElementsToWrite = (size_t*)malloc(n_threads * sizeof(size_t));
    pthread_t tClientID[n_threads];
    struct arg* threadArgs = (struct arg*)malloc(n_threads * sizeof(struct arg));
    pthread_barrier_init(&barrier, NULL, n_threads);
    pthread_mutex_init(&a_mutex, NULL);

    //Divide up work
    for (size_t i = 0; i < n_threads; i++)
    {
        threadArgs[i].id = i;
        threadArgs[i].start = i * char_segment_size;
        threadArgs[i].end = (i + 1) * char_segment_size;
        //pthread_create(thread, attr, start_routine, arg);
        pthread_create(&tClientID[i], NULL, do_loop, (void*)&threadArgs[i]);
    }

    for (size_t i = 0; i < n_threads; i++)
    {
        pthread_join(tClientID[i], NULL);
    }

    free(threadArgs);
    free(numElementsToWrite);
    pthread_barrier_destroy(&barrier);
}

void* do_loop(void* data)
{
    struct arg* arguments = (struct arg*)data;
    int numunique = 0;
    int i;
    int offset = 'a'; // ASCII 97
    int tid = arguments->id;     /* thread identifying number */
    // Get first element from input chars and put it in the head of
    // our linked list.
    nodeLink head = (nodeLink)malloc(sizeof(struct node));
    struct zipped_char temp_zip;
    temp_zip.character = input_chars_local[arguments->start];
    temp_zip.occurence = 1;
    head->value = temp_zip;
    head->next = NULL;
    nodeLink tail = head;
    ++numunique;

    // count letters
    for (i = arguments->start + 1; i < arguments->end; ++i)
    {
        char currChar = input_chars_local[i];
        if (tail->value.character == currChar)
        {
            ++(tail->value.occurence);
        }
        else
        {
            nodeLink temp_link = (nodeLink)malloc(sizeof(struct node));
            temp_link->value.character = currChar;
            temp_link->value.occurence = 1;
            tail->next = temp_link;
            tail = tail->next;
            ++numunique;
        }
    }

    //update char_frequency
    int rc = pthread_mutex_lock(&a_mutex);
    if (rc)
    { /* an error has occurred */
        perror("pthread_mutex_lock");
        pthread_exit(NULL);
    }
    /*** Start Critical Section ***
     * mutex is now locked - do your stuff.
     */

    nodeLink curr = head;
    for (i = 0; (i < numunique) && (curr != NULL); ++i)
    {
        char currChar = curr->value.character;
        int index = currChar - offset;
        char_frequency_local[index] = char_frequency_local[index] + curr->value.occurence; //add in our results
        curr = curr->next;
    }
    /*** End Critical Section ***/
    rc = pthread_mutex_unlock(&a_mutex);
    if (rc)
    {
        perror("pthread_mutex_unlock");
        pthread_exit(NULL);
    } // unlock mutex

    /****** IMPORTANT ******/
    // This line MUST occur before the barrier.
    numElementsToWrite[tid] = numunique;
    /****** IMPORTANT ******/

    pthread_barrier_wait(&barrier);

    rc = pthread_mutex_lock(&a_mutex);
    if (rc)
    { /* an error has occurred */
        perror("pthread_mutex_lock");
        pthread_exit(NULL);
    }
    /*** Start Critical Section ***/
    /* Write to zipped chars */

    // Determine what location in the zipped_chars array
    // that thread tid should start writing to.
    int start = 0;
    for (int i = 0; i < tid; ++i)
    {
        start += numElementsToWrite[i];
    }
    curr = head;
    for (int i = 0; (i < numunique) && (curr != NULL); ++i)
    {
        zipped_char_local[i + start] = curr->value;
        ++(*zipped_chars_count_local);
        curr = curr->next;
    }
    startOutputIndex += numunique;
    /*** End Critical Section ***/
    rc = pthread_mutex_unlock(&a_mutex);
    if (rc)
    {
        perror("pthread_mutex_unlock");
        pthread_exit(NULL);
    } // unlock mutex

    // free linked list nodes
    curr = head;
    while (curr != NULL)
    {
        nodeLink temp = curr->next;
        free(curr);
        curr = temp;
    }


    return 0;
}
//end do_loop

