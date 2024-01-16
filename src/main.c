#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <time.h>

// Show execution time
#define SHOW_TIME

// Show data at each stage
// #define SHOW_DATA

#define DATA_SIZE (size_t) 1000 * 1000

typedef struct {
    size_t *data;
    size_t size;
} Data;

typedef struct {
    bool created;
    pthread_t thread;
    size_t index;
    Data data;
} Thread;

size_t randst(void) {
    size_t result = 0;
    int r;

    for (size_t i = 0; i < sizeof(size_t); i++) {
        r = rand() >> (8 * (sizeof(int) - 1));
        result |= (r << (8 * i));
    }

    return result;
}

size_t randst_between(size_t lower, size_t upper) {
    return lower + (randst() % upper);
}

void print_time(clock_t start, clock_t end) {
#ifdef SHOW_TIME
    printf("- Took %lf ms\n",
        (1000 * (double) (end - start)) / CLOCKS_PER_SEC
    );
#else
    (void) start;
    (void) end;
#endif
}

void generate_data(Data *data) {
    clock_t start = clock(), end;

    for (size_t i = 0; i < data->size; i++) {
        data->data[i] = i;
    }

    end = clock();
    print_time(start, end);
}

void shuffle_data(Data *data) {
    clock_t start = clock(), end;
    size_t temp;
    size_t index;

    for (size_t i = 0; i < data->size; i++) {
        index = randst_between(0, data->size);

        temp = data->data[i];
        data->data[i] = data->data[index];
        data->data[index] = temp;
    }

    end = clock();
    print_time(start, end);
}

void print_data(Data data) {
#ifdef SHOW_DATA
    puts("{");

    for (size_t i = 0; i < data.size; i++) {
        printf("%5zu", data.data[i]);

        if (i + 1 < data.size) {
            printf(", ");
            if ((i + 1) % 10 == 0) puts("");
        }
    }

    puts("\n}");
#else
    (void) data;
#endif
}

void merge(Data *data, Data left, Data right) {
    size_t li = 0;
    size_t ri = 0;

    assert(left.size + right.size == data->size);

    for (size_t i = 0; i < data->size; i++) {
        if      (li >= left.size)                data->data[i] = right.data[ri++];
        else if (ri >= right.size)               data->data[i] = left.data[li++];
        else if (left.data[li] < right.data[ri]) data->data[i] = left.data[li++];
        else                                     data->data[i] = right.data[ri++];
    }
}

void merge_sort(Data *data) {
    size_t ln = data->size / 2;
    size_t rn = data->size - ln;

    size_t arr_left[ln];
    size_t arr_right[rn];

    Data left  = { .data = arr_left,  .size = ln };
    Data right = { .data = arr_right, .size = rn };

    for (size_t i = 0; i < left.size; i++) {
        left.data[i] = data->data[i];
    }

    for (size_t i = 0; i < right.size; i++) {
        right.data[i] = data->data[ln + i];
    }

    if (left.size > 1)  merge_sort(&left);
    if (right.size > 1) merge_sort(&right);

    merge(data, left, right);
}

void validate_order(Data data) {
    clock_t start = clock(), end;
    size_t prev;
    size_t curr;

    if (data.size < 1) return;

    prev = data.data[0];

    for (size_t i = 1; i < data.size; i++) {
        curr = data.data[i];

        if (curr <= prev) {
            fprintf(stderr,
                "At index %zu. Elements out of order: %zu, %zu\n",
                i, prev, curr
            );
            return;
        }

        prev = curr;
    }

    end = clock();
    print_time(start, end);
}

void single_thread(Data *data) {
    clock_t start, end;

    puts("\n=== Sorting array (single-thread) ===");

    start = clock();

    merge_sort(data);
    end = clock();

    print_time(start, end);
}

void *thread_merge_sort(void *data) {
    merge_sort(data);
    return NULL;
}

void multi_merge(Thread *threads, int threads_size, Data *data) {
    bool lowest_set;
    size_t lowest, lowest_index = 0;
    Thread *thread;

    // Final merge
    for (size_t i = 0; i < data->size; i++) {
        lowest = 0;
        lowest_set = false;

        for (int j = 0; j < threads_size; j++) {
            thread = &(threads[j]);

            if (thread->index >= thread->data.size) continue;
            if (lowest_set == true
                && thread->data.data[thread->index] >= lowest) continue;

            lowest_set = true;

            lowest = thread->data.data[thread->index];
            lowest_index = j;
        }

        data->data[i] = lowest;
        threads[lowest_index].index++;
    }
}

void multi_thread(Data *data) {
    int result;
    clock_t start, end;

    int nprocs = get_nprocs();
    size_t chunk_size = data->size / nprocs;
    size_t chunk_rem = data->size % nprocs;
    size_t arr[data->size];
    Thread *threads = calloc(nprocs, sizeof(Thread));
    pthread_attr_t attrs;

    if (threads == NULL) {
        perror("calloc");
        return;
    }

    // Copy the data
    for (size_t i = 0; i < data->size; i++) {
        arr[i] = data->data[i];
    }

    if (pthread_attr_init(&attrs) != 0) {
        perror("pthread_attr_init");
        return;
    }

    if (pthread_attr_setstacksize(&attrs, 4 * 8192 * 1024) != 0) {
        perror("pthread_attr_setstacksize");
        return;
    }

    // Set all threads to uncreated
    for (int i = 0; i < nprocs; i++) {
        threads[i].created = false;
    }

    for (int i = 0; i < nprocs; i++) {
        threads[i].data.data = arr + (i * chunk_size);
        threads[i].data.size = chunk_size;

        if (i + 1 >= nprocs) threads[i].data.size += chunk_rem;

        result = pthread_create(
            &(threads[i].thread), &attrs,
            thread_merge_sort, &(threads[i].data)
        );

        if (result == 0) continue;

        fprintf(stderr,
            "Thread %d: pthread_create: %s\n", i, strerror(result)
        );

        goto cleanup;
    }

    puts("\n=== Sorting array (multi-thread) ===");

    start = clock();

    for (int i = 0; i < nprocs; i++) {
        result = pthread_join(threads[i].thread, NULL);
        if (result == 0) continue;

        fprintf(stderr,
            "Thread %d: pthread_join: %s\n", i, strerror(result)
        );

        goto cleanup;
    }

    multi_merge(threads, nprocs, data);

    end = clock();

    print_time(start, end);

cleanup:
    for (int i = 0; i < nprocs; i++) {
        if (threads[i].created == false) continue;

        result = pthread_cancel(threads[i].thread);
        if (result == 0) continue;

        fprintf(stderr,
            "Thread %d: pthread_cancel: %s\n", i, strerror(result)
        );
    }

    free(threads);
}

int main(void) {
    size_t arr[DATA_SIZE] = {0};

    Data data = {
        .data = arr,
        .size = DATA_SIZE,
    };

    assert(data.size > 0);
    srand(time(NULL));

    // Generate
    printf("=== Generating array (%zu elements) ===\n", data.size);
    generate_data(&data);
    print_data(data);

    // Shuffle
    puts("\n=== Shuffling array ===");
    shuffle_data(&data);
    print_data(data);

    // Sort
#if 0
    single_thread(&data);
#else
    multi_thread(&data);
#endif

    print_data(data);

    // Validate
    puts("\n=== Validating order ===");
    validate_order(data);
}
