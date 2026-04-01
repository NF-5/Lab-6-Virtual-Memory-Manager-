#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define PAGE_SIZE 256
#define PAGE_TABLE_SIZE 256
#define TLB_SIZE 16
#define FRAME_SIZE 256
#define FRAME_COUNT 128
#define PHYSICAL_MEMORY_SIZE (FRAME_COUNT * FRAME_SIZE)

typedef struct {
    int page_number;
    int frame_number;
    int valid;
} TLBEntry;

static signed char physical_memory[PHYSICAL_MEMORY_SIZE];
static int page_table[PAGE_TABLE_SIZE];
static TLBEntry tlb[TLB_SIZE];

/* FIFO page replacement support */
static int frame_to_page[FRAME_COUNT];
static int next_free_frame = 0;
static int fifo_pointer = 0;

/* FIFO TLB replacement */
static int next_tlb_index = 0;

/* Stats */
static int total_addresses = 0;
static int page_faults = 0;
static int tlb_hits = 0;

void initialize_page_table(void) {
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        page_table[i] = -1;
    }
}

void initialize_tlb(void) {
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].page_number = -1;
        tlb[i].frame_number = -1;
        tlb[i].valid = 0;
    }
}

void initialize_frame_table(void) {
    for (int i = 0; i < FRAME_COUNT; i++) {
        frame_to_page[i] = -1;
    }
}

int search_tlb(int page_number) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].page_number == page_number) {
            return tlb[i].frame_number;
        }
    }
    return -1;
}

void remove_page_from_tlb(int page_number) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].page_number == page_number) {
            tlb[i].valid = 0;
            tlb[i].page_number = -1;
            tlb[i].frame_number = -1;
        }
    }
}

void add_to_tlb(int page_number, int frame_number) {
    tlb[next_tlb_index].page_number = page_number;
    tlb[next_tlb_index].frame_number = frame_number;
    tlb[next_tlb_index].valid = 1;
    next_tlb_index = (next_tlb_index + 1) % TLB_SIZE;
}

int load_page_into_frame(FILE *backing_store, int page_number, int frame_number) {
    long offset = (long)page_number * PAGE_SIZE;

    if (fseek(backing_store, offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error: Failed to seek in BACKING_STORE.bin\n");
        exit(EXIT_FAILURE);
    }

    size_t bytes_read = fread(&physical_memory[frame_number * FRAME_SIZE],
                              sizeof(signed char),
                              PAGE_SIZE,
                              backing_store);

    if (bytes_read != PAGE_SIZE) {
        fprintf(stderr, "Error: Failed to read page from BACKING_STORE.bin\n");
        exit(EXIT_FAILURE);
    }

    return frame_number;
}

int handle_page_fault(FILE *backing_store, int page_number) {
    int frame_number;

    /* If there is still free space, use the next free frame */
    if (next_free_frame < FRAME_COUNT) {
        frame_number = next_free_frame;
        next_free_frame++;
    } else {
        /* FIFO replacement */
        frame_number = fifo_pointer;
        int victim_page = frame_to_page[frame_number];

        if (victim_page != -1) {
            page_table[victim_page] = -1;
            remove_page_from_tlb(victim_page);
        }

        fifo_pointer = (fifo_pointer + 1) % FRAME_COUNT;
    }

    load_page_into_frame(backing_store, page_number, frame_number);

    page_table[page_number] = frame_number;
    frame_to_page[frame_number] = page_number;

    return frame_number;
}

int main(void) {
    FILE *backing_store = fopen("BACKING_STORE.bin", "rb");
    if (backing_store == NULL) {
        fprintf(stderr, "Error: Could not open BACKING_STORE.bin\n");
        return EXIT_FAILURE;
    }

    initialize_page_table();
    initialize_tlb();
    initialize_frame_table();

    int logical_address_input;

    while (scanf("%d", &logical_address_input) == 1) {
        total_addresses++;

        int logical_address = logical_address_input & 0xFFFF;
        int page_number = (logical_address >> 8) & 0xFF;
        int offset = logical_address & 0xFF;

        int frame_number = search_tlb(page_number);

        if (frame_number != -1) {
            tlb_hits++;
        } else {
            frame_number = page_table[page_number];

            if (frame_number == -1) {
                page_faults++;
                frame_number = handle_page_fault(backing_store, page_number);
            }

            add_to_tlb(page_number, frame_number);
        }

        int physical_address = frame_number * FRAME_SIZE + offset;
        signed char value = physical_memory[physical_address];

        printf("Virtual address: %d Physical address: %d Value: %d\n",
               logical_address_input,
               physical_address,
               value);
    }

    fclose(backing_store);

    double page_fault_rate = 0.0;
    double tlb_hit_rate = 0.0;

    if (total_addresses > 0) {
        page_fault_rate = (double)page_faults / (double)total_addresses;
        tlb_hit_rate = (double)tlb_hits / (double)total_addresses;
    }

    printf("Page Fault Rate = %.3f\n", page_fault_rate);
    printf("TLB Hit Rate = %.3f\n", tlb_hit_rate);

    return EXIT_SUCCESS;
}