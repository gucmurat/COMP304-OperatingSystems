/**
 * virtmem.c 
 */

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define TLB_SIZE 16
#define PAGES 1024
//1111 1111 1100 0000 0000
#define PAGE_MASK 0xFFC00 /* TODO */

#define PAGE_SIZE 1024
#define OFFSET_BITS 10
//0000 0000 0011 1111 1111
#define OFFSET_MASK 0x003FF /* TODO */

#define MEMORY_SIZE PAGES * PAGE_SIZE

// Max number of characters per line of input file to read.
#define BUFFER_SIZE 10

struct tlbentry {
  unsigned int logical;
  unsigned int physical;
};

// TLB is kept track of as a circular array, with the oldest element being overwritten once the TLB is full.
struct tlbentry tlb[TLB_SIZE];
// number of inserts into TLB that have been completed. Use as tlbindex % TLB_SIZE for the index of the next TLB line to use.
int tlbindex = 0;

// pagetable[logical_page] is the physical page number for logical page. Value is -1 if that logical page isn't yet in the table.
int pagetable[PAGES];

//An array to keep recently reached order
int lru[256];

signed char main_memory[MEMORY_SIZE / 4];

// Pointer to memory mapped backing file
signed char *backing;

int max(int a, int b)
{
  if (a > b)
    return a;
  return b;
}

/* Returns the physical address from TLB or -1 if not present. */
int search_tlb(unsigned int logical_page) {
    /* TODO */
    int i;
    for (i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].logical == logical_page) {
            //Update the LRU array to show we recently reached the TLB hitted location
            for(int i = 0; i < 256; i++){
                //Increase the recently reached times if they reached more recently than the current page
                if(lru[i] <= logical_page){
                    lru[i] = (lru[i] + 1) % 256;
                }
            }
            lru[logical_page] = 0;
            return tlb[i].physical;
        }
    }
    return -1;
}

int search_tlb_physical(unsigned int physical_page){
    int i;
    for (i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].physical == physical_page) {
            return tlb[i].logical;
        }
    }
    return -1;
}

/* Adds the specified mapping to the TLB, replacing the oldest mapping (FIFO replacement). */
void add_to_tlb(unsigned int logical, unsigned int physical) {
    /* TODO */
    tlb[tlbindex % TLB_SIZE].logical = logical;
    tlb[tlbindex % TLB_SIZE].physical = physical;
    tlbindex++;
}

void update_tlb (unsigned int logical_old, unsigned int logical_new, unsigned int physical) {
  int i;
    for (i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].logical == logical_old) {
            tlb[i].logical = logical_new;
            tlb[i].physical = physical;
        }
    }
}
int main(int argc, const char *argv[])
{
  if (argc != 5) {
    fprintf(stderr, "Usage ./virtmem backingstore input -p policy(0/1)\n");
    exit(1);
  }
  int mode=-1;
  for(int i=1; i<argc; i++){
        if(!strcmp(argv[i], "-p")) 
        {mode = atoi(argv[++i]);}
  }
  if(mode==-1){
    fprintf(stderr, "Usage ./virtmem backingstore input -p policy(0/1)\n");
    exit(1);
  } 
  
  const char *backing_filename = argv[1]; 
  int backing_fd = open(backing_filename, O_RDONLY);
  backing = mmap(0, MEMORY_SIZE, PROT_READ, MAP_PRIVATE, backing_fd, 0); 
  
  const char *input_filename = argv[2];
  FILE *input_fp = fopen(input_filename, "r");
  
  // Fill page table entries with -1 for initially empty table.
  int i;
  for (i = 0; i < PAGES; i++) {
    pagetable[i] = -1;
  }
  
  int j;
  for (j = 0; j < 256; j++) {
    lru[j] = 255 - j;
  }
  
  // Character buffer for reading lines of input file.
  char buffer[BUFFER_SIZE];
  
  // Data we need to keep track of to compute stats at end.
  int total_addresses = 0;
  int tlb_hits = 0;
  int page_faults = 0;
  
  // Number of the next unallocated physical page in main memory
  unsigned int free_page = 0;
  while (fgets(buffer, BUFFER_SIZE, input_fp) != NULL) {
    int k;
    //For LRU 
    for (k = 0; k < 256; k++) {
        if(lru[k] == 255 && mode == 1){
            free_page = k;
        }
    }
    int tlb_update_flag = 0;
    total_addresses++;
    int logical_address = atoi(buffer);

    /* TODO 
    / Calculate the page offset and logical page number from logical_address */
    int offset = logical_address & OFFSET_MASK;
    int logical_page = (logical_address & PAGE_MASK) >> OFFSET_BITS;
    ///////
    int physical_page = search_tlb(logical_page);
    // TLB hit
    if (physical_page != -1) {
      tlb_hits++;
      // TLB miss
    } else {
      physical_page = pagetable[logical_page];
      
      // Page fault
      if (physical_page == -1) {
          /* TODO */
          //Copy from hard disk to main memory
          memcpy(main_memory + (free_page * PAGE_SIZE), backing + (logical_page * PAGE_SIZE), PAGE_SIZE);
          //Clear the previous pointers to spesific main memory
          for (int i = 0; i < 1024; i++) {
            if(pagetable[i] == free_page){
              pagetable[i] = -1;
            }
          }

          //Clear the TLB from previous pointers
          int old_logical = search_tlb_physical(free_page);
          if( old_logical != -1){
                update_tlb(old_logical,logical_page,free_page);
                tlb_update_flag = 1;
                
          }
          
          //Update the page table
          pagetable[logical_page] = free_page;
          physical_page = free_page;
          //For FIFO
          if(mode==0){
            free_page = (free_page + 1) % 256;
          }
           //Increase the recently reached times - For LRU
          for(int i = 0; i < 256; i++){
            lru[i] = (lru[i] + 1) % 256;
          }
          page_faults++;
      }else{
        //Update the LRU array to show we recently reached the from the page table
        for(int i = 0; i < 256; i++){
            //Increase the recently reached times if they reached more recently than the current page
            if(lru[i] <= logical_page){
                lru[i] = (lru[i] + 1) % 256;
            }
        }
        lru[logical_page] = 0;
      }
      //Update the TLB
      if(tlb_update_flag == 0){
        add_to_tlb(logical_page, physical_page);
      }
    }
    
    int physical_address = (physical_page << OFFSET_BITS) | offset;
    signed char value = main_memory[physical_page * PAGE_SIZE + offset];
    
    printf("Virtual address: %d Physical address: %d Value: %d\n", logical_address, physical_address, value);
  }
  
  printf("Number of Translated Addresses = %d\n", total_addresses);
  printf("Page Faults = %d\n", page_faults);
  printf("Page Fault Rate = %.3f\n", page_faults / (1. * total_addresses));
  printf("TLB Hits = %d\n", tlb_hits);
  printf("TLB Hit Rate = %.3f\n", tlb_hits / (1. * total_addresses));
  
  return 0;
}