/**
Team Details
Ravali Kandur (kandu009)
Charandeep Parisineti (paris102)
*/

#include "disk.h"
#include <errno.h>
#include "page_table.h"
#include "program.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * struct used to hold the set of input arguments.
 */
struct arguments_struct {
  int num_pages;				  // number of pages in input arguments.
  int num_frames;			    // number of frames in input arguments.
  const char *algorithm;	// algorithm mentioned in input arguments.
  const char *program;	  // program mentioned in input arguments.
};

/**
 * enum used to denote all the supported page replacement algorithms.
 */
enum enum_replacement_algorithm { RAND, FIFO, CUSTOM };

/**
 * struct to denote the frame structure which is used to access the pages directly, instead of acccessing it from the disk.
 */
struct frame_entry_struct {
  int is_free;									          // = 1 if the frame is free, 0 otherwise.
  int read_write_bits;							      // contains the permission bits like PROT_READ/WRITE.
  int frame_type; 								        // indicates the type of the frame to identify if its free to be used.
  int page_id;									          // Page Id which this frame is representing.
  struct frame_entry_struct * next_frame_entry;	// next frame entry, used in FIFO like ordering.
  struct frame_entry_struct * prev_frame_entry;	// previous frame entry, used in FIFO like ordering.
};

/**
 * struct which holds all the stats for each run.
 */
struct statistics_struct {
  int disk_reads_count;	  // number of disk reads.
  int disk_writes_count;	// number of disk writes.
  int page_faults_count;	// number of page faults.
  int evictions_count;	  // number of evictions. This is not asked in the paper, but we were curious to see how this behaves.
};

// different page fault handlers.
void rand_pagefault_handler( struct page_table *pt, int page );
void fifo_pagefault_handler( struct page_table *pt, int page );
void custom_pagefault_handler( struct page_table *pt, int page );

// helper methods.
void replace_page(struct page_table * pt, int f_num);	// to evict a page when we run out of resources.
void insert_frame_into_fifo(int frame_index);				  // inserts a new frame based on fifo ordering.
int  remove_frame_from_fifo();								        // removes a new frame based on fifo ordering.
int get_unused_frame();							                  // gets a frame which is not already occupied.
int get_non_dirty_frame();						                // gets a frame which is not already marked dirty.
void print_resource_usage_stats();				            // prints the stats collected at the end of each run.

// global variables which will be used across functions.
struct disk *disk = NULL;							// mimic of the actual disk.
char *virtmem = NULL;								// mimic of the actual virtual memory.
char *physmem = NULL;								// mimic of the actual physical memory.

struct arguments_struct args;						// input arguments struct.
enum enum_replacement_algorithm replacement_algo;	// enum to represent the replacement algrithm in use.
struct statistics_struct vm_stats;					// struct for holding computed stats.
struct frame_entry_struct *frames_store = NULL;		// frames store which is used for the easy/random access.
struct frame_entry_struct *fifo_head = NULL;		// frame which denotes the head of the FIFO queue in FIFO replacement algorithm.
struct frame_entry_struct *fifo_tail = NULL;		// frame which denotes the tail of the FIFO queue in FIFO replacement algorithm.
int wait_limit = 0;									// threshold variable to intend how long can we wait in the custom algorithm.

// based method which is called when a page fault occurs.
void page_fault_handler( struct page_table *pt, int page ) {
  // track the page faults.
  ++vm_stats.page_faults_count;
  switch (replacement_algo) {
    case RAND: {
                 rand_pagefault_handler(pt, page);
                 break;
               }
    case FIFO: {
                 fifo_pagefault_handler(pt, page);
                 break;
               }
    case CUSTOM: {
                   custom_pagefault_handler(pt, page);
                   break;
                 }
    default: {
               printf("unsupported page fault #%d occurred, exiting! \n",page);
               exit(1);
             }
  }
}

int main( int argc, char *argv[] ) {

  // input validation.
  if(argc!=5) {
    printf("usage: ./virtmem <num_pages> <num_frames> <rand|fifo|custom> <sort|scan|focus>\n");
    exit(1);
  }
  // set the input arguments needed.
  memset(&args, 0, sizeof(struct arguments_struct));
  args.num_pages  = atoi(argv[1]);
  args.num_frames = atoi(argv[2]);
  args.algorithm  = argv[3];
  args.program = argv[4];

  // validate pages and frames.
  if (args.num_pages < 1) {
    printf("number of pages must be at least 1.\n");
    exit(1);
  } else if (args.num_frames < 1) {
    printf("number of frames must be at least 1.\n");
    exit(1);
  }
  // our assumption is that number of frames cannot be greater than number of pages.
  if(args.num_frames > args.num_pages) {
    printf("number of frames must be <= number of pages.\n");
    exit(1);
  }

  // check if the program given is supported.
  if((0 != strcmp(args.program,"sort")) && (0 != strcmp(args.program,"scan")) && (0 != strcmp(args.program,"focus"))) {
    printf("usage: ./virtmem <num_pages> <num_frames> <rand|fifo|custom> <sort|scan|focus>\n");
    exit(1);
  }

  // validate if the replacement algorithm given is supported.
  if (!strcmp(args.algorithm,"rand")) {
    replacement_algo = RAND;
  } else if (!strcmp(args.algorithm,"fifo")) {
    replacement_algo = FIFO;
  } else if (!strcmp(args.algorithm,"custom")) {
    replacement_algo = CUSTOM;
  } else {
    printf("usage: ./virtmem <num_pages> <num_frames> <rand|fifo|custom> <sort|scan|focus>\n");
    exit(1);
  }

  // initialize the disk.
  disk = disk_open("myvirtualdisk",args.num_pages);
  if(!disk) {
    fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
    exit(1);
  }
  // initialize page table.
  struct page_table *pt = page_table_create( args.num_pages, args.num_frames, page_fault_handler );
  if(!pt) {
    fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
    exit(1);
  }
  // initialize physical memory.
  physmem = page_table_get_physmem(pt);
  // initialize virtual memory.
  virtmem = page_table_get_virtmem(pt);

  // initialize frame store.
  frames_store = malloc(args.num_frames * sizeof(struct frame_entry_struct));
  if (frames_store == NULL) {
    printf("cannot create a store, running out of memory?\n");
    exit(1);
  }
  memset(frames_store, 0, args.num_frames * sizeof(struct frame_entry_struct));

  // initialize the stats store.
  memset(&vm_stats, 0, sizeof(struct statistics_struct));

  // a calculated threshold which is used to wait until we choose our frame to be replaced in the custom algorithm.
  wait_limit = args.num_frames * 5/6;

  // invoke the respective program.
  if(!strcmp(args.program,"sort")) {
    sort_program(virtmem,args.num_pages*PAGE_SIZE);
  } else if(!strcmp(args.program,"scan")) {
    scan_program(virtmem,args.num_pages*PAGE_SIZE);
  } else if(!strcmp(args.program,"focus")) {
    focus_program(virtmem,args.num_pages*PAGE_SIZE);
  }

  // print stats.
  print_resource_usage_stats();

  // cleanup resources.
  free(frames_store);
  page_table_delete(pt);
  disk_close(disk);

  return 0;
}

/**
 * method which handles the random page fault.
 */
void rand_pagefault_handler( struct page_table *pt, int page ) {

  int current_permissions = 0;
  int current_frame = 0;
  page_table_get_entry(pt, page, &current_frame, &current_permissions);

  // now we need to find the page/frame which can be replaced with the current page.
  int replace_frame_index = -1;

  // this mean we do not even have read bits.
  if (!current_permissions) {
    // add read permission to the new frame being added.
    current_permissions |= PROT_READ;
    // we are forced to replace with something already existing at random.
    replace_frame_index = (int)lrand48() % args.num_frames;
    if(frames_store[replace_frame_index].is_free != 0) {
      replace_page(pt, replace_frame_index);
    }
    // we are making a new disk read now.
    ++vm_stats.disk_reads_count;
    disk_read(disk, page, &physmem[replace_frame_index * PAGE_SIZE]);
    // this means we have read bit set, and the request is for writing.
  } else if ((current_permissions & PROT_READ) && !(current_permissions & PROT_WRITE)) {
    // add write permissions.
    current_permissions |= PROT_WRITE;
    replace_frame_index = current_frame;
  } else {
    // this should ideally not happen.
    printf("encountered unexpected situation in random page fault handler.\n");
    // do not exit as we need to clean up resources.
    return;
  }

  // post the new changes to page table.
  page_table_set_entry(pt, page, replace_frame_index, current_permissions);
  // post the new changes to our local frame store.
  frames_store[replace_frame_index].page_id = page;
  frames_store[replace_frame_index].read_write_bits = current_permissions;
  frames_store[replace_frame_index].is_free = 1;
}

/**
 * method which handles the page fault using FIFO algorithm.
 */
void fifo_pagefault_handler( struct page_table *pt, int page ) {

  int current_permissions = 0;
  int current_frame = 0;
  page_table_get_entry(pt, page, &current_frame, &current_permissions);

  // now we need to find the page/frame which can be replaced with the current page.
  int replace_frame_index = -1;

  // this mean we do not even have read bits.
  if (!current_permissions) {
    // add read permission to the new frame being added.
    current_permissions |= PROT_READ;
    // check if there are any unused frames.
    if ((replace_frame_index = get_unused_frame()) < 0) {
      // if there are none, then we are forced to replace with a frame using FIFO order.
      if ((replace_frame_index = remove_frame_from_fifo()) < 0) {
        printf("encountered unexpected situation in FIFO page fault handler.\n");
        // do not exit as we need to clean up resources.
        return;
      }
      replace_page(pt, replace_frame_index);
    }
    // we are making a new disk read now.
    ++vm_stats.disk_reads_count;
    disk_read(disk, page, &physmem[replace_frame_index * PAGE_SIZE]);
    // this means we have read bit set, and the request is for writing.
  } else if ( (current_permissions & PROT_READ) && !(current_permissions & PROT_WRITE)) {
    // add write permissions.
    current_permissions |= PROT_WRITE;
    replace_frame_index = current_frame;
  } else {
    // this should ideally not happen.
    printf("encountered unexpected situation in FIFO page fault handler.\n");
    // do not exit as we need to clean up resources.
    return;
  }

  // post the new changes to page table.
  page_table_set_entry(pt, page, replace_frame_index, current_permissions);
  // post the new changes to our local frame store.
  frames_store[replace_frame_index].page_id = page;
  frames_store[replace_frame_index].read_write_bits = current_permissions;
  frames_store[replace_frame_index].is_free = 1;
  // insert into our FIFO queue.
  insert_frame_into_fifo(replace_frame_index);

}

/**
 * method which handles the page fault using our custom algorithm.
 * the custom algorithm uses the following approach in the same order
 * 1. looks for an unused frame if there is one.
 * 2. looks for a frame which is not marked dirty
 * 3. looks for the best possible frame which can be replaced based on a threshold.
 */
void custom_pagefault_handler( struct page_table *pt, int page ) {

  int current_permissions = 0;
  int current_frame = 0;
  page_table_get_entry(pt, page, &current_frame, &current_permissions);

  // now we need to find the page/frame which can be replaced with the current page.
  int replace_frame_index = -1;

  // this mean we do not even have read bits.
  if (!current_permissions) {
    // add read permission to the new frame being added.
    current_permissions |= PROT_READ;
    // check if there are any unused frames.
    // if there are none, then we are forced to replace with a frame which is free.
    if ((replace_frame_index = get_unused_frame()) < 0) {
      // if there are none, look for a frame which is not marked dirty yet.
      if ((replace_frame_index = get_non_dirty_frame()) < 0) {
        // if there are none, then we are forced to replace with a frame using FIFO algorithm.
        if ((replace_frame_index = remove_frame_from_fifo()) < 0) {
          printf("encountered unexpected situation in FIFO page fault handler.\n");
          // do not exit as we need to clean up resources.
          return;
        }
      }
      replace_page(pt, replace_frame_index);
    }
    // we are making a new disk read now.
    ++vm_stats.disk_reads_count;
    disk_read(disk, page, &physmem[replace_frame_index * PAGE_SIZE]);
    // this means we have read bit set, and the request is for writing.
  } else if (current_permissions & PROT_READ && !(current_permissions & PROT_WRITE)) { // Missing write bit
    // add write permissions.
    current_permissions |= PROT_WRITE;
    replace_frame_index = current_frame;
  } else {
    // this should ideally not happen.
    printf("encountered unexpected situation in custom page fault handler.\n");
    // do not exit as we need to clean up resources.
    return;
  }

  // post the new changes to page table.
  page_table_set_entry(pt, page, replace_frame_index, current_permissions);

  // post the new changes to our local frame store.
  frames_store[replace_frame_index].page_id = page;
  frames_store[replace_frame_index].read_write_bits = current_permissions;
  frames_store[replace_frame_index].is_free = 1;
  // insert into our FIFO queue.
  insert_frame_into_fifo(replace_frame_index);

}

/**
 * method which gets the frame index that is unoccupied/unused.
 */
int get_unused_frame() {
  for (int i = 0; i < args.num_frames; ++i) {
    if (frames_store[i].is_free == 0) return i;
  }
  return -1;
}

/**
 * method which inserts a new node at the tail of the FIFO chain.
 */
void insert_frame_into_fifo(int index) {

  // since our frame store is already updated with new frame. use this.
  struct frame_entry_struct * new_frame = &frames_store[index];
  // if the new frame is already in the FIFO chain, then we don't have to add it again.
  if (new_frame->frame_type == 1) {
    return;
  }

  // this means new_frame is the first element in the FIFO chain.
  if (fifo_tail == NULL) {
    fifo_head = new_frame;
    fifo_tail = new_frame;
    new_frame->frame_type = 1;
  } else {
    // else add the new element at the tail of the FIFO chain.
    new_frame->next_frame_entry = fifo_tail;
    new_frame->prev_frame_entry = NULL;
    fifo_tail-> prev_frame_entry = new_frame;
    fifo_tail  = new_frame;
    new_frame->frame_type = 1;
  }
}

/**
 * method which removes a node at the head of the FIFO chain.
 */
int remove_frame_from_fifo() {
  // this means there are no elements in the FIFO chain.
  if (fifo_head == NULL) {
    return -1;
  }
  // this means there is only 1 element in the FIFO chain.
  if (fifo_tail == fifo_head) {
    fifo_head = NULL;
    fifo_tail = NULL;
    fifo_head->frame_type = 0;
    return ((int) (fifo_head - frames_store));
  }
  // else, remove the frame at the head of FIFO chain.
  fifo_head->frame_type = 0;
  int replace_frame_index = ((int) (fifo_head - frames_store));
  fifo_head = fifo_head->prev_frame_entry;
  fifo_head->next_frame_entry = NULL;
  return replace_frame_index;
}

/**
 * method to check if there is any frame which is not marked dirty yet.
 */
int get_non_dirty_frame() {

  // our ultimate result would be in this.
  struct frame_entry_struct * chosen_frame = NULL;
  // iterator to traverse all the frames.
  struct frame_entry_struct * current_frame = fifo_tail->next_frame_entry;
  int current_limit = 0;

  // just do a linear traversal through the elements in the list until our threshold.
  while (current_limit < wait_limit && current_frame != NULL) {
    // update the chosen frame until we reach the threshold.
    if (current_frame->read_write_bits & (~PROT_WRITE)) {
      chosen_frame = current_frame;
      current_limit++;
    }
    current_frame = current_frame->next_frame_entry;
  }

  if ( chosen_frame == NULL ) {
    // there is no frame available.
    return -1;
  }

  current_frame = chosen_frame;
  // if the chosen element is at the tail.
  if (current_frame == fifo_tail) {
    // update the new tail previous and next elements.
    fifo_tail = fifo_tail->next_frame_entry;
    if (fifo_tail != NULL) {
      fifo_tail->prev_frame_entry = NULL;
    } else {
      // this means we have only one element in the FIFO chain.
      fifo_head = fifo_tail;
    }
    //reset the frame type and return the index value.
    current_frame->frame_type = 0;
    return ((int) (current_frame - frames_store));
  }

  // this means we have chosen some node well before the tail.
  // this is as good as removing a node in the middle of a linked list.
  if (current_frame->next_frame_entry != NULL) {
    current_frame->next_frame_entry->prev_frame_entry = current_frame->prev_frame_entry;
  }
  current_frame->prev_frame_entry->next_frame_entry = current_frame->next_frame_entry;
  //reset the frame type and return the index value.
  current_frame->frame_type = 0;
  return ((int) (current_frame - frames_store));

}

/**
 * method which is used to replace an existing frame with a new page.
 */
void replace_page(struct page_table * pt, int f_num) {
  // if the frame is in write mode, then we might assume that it is dirty.
  if (frames_store[f_num].read_write_bits & PROT_WRITE) {
    // we first need to write the old data to disk.
    ++vm_stats.disk_writes_count;
    disk_write(disk, frames_store[f_num].page_id, &physmem[f_num * PAGE_SIZE]);
  }
  // update the page table and frame store with the new page.
  page_table_set_entry(pt, frames_store[f_num].page_id, f_num, PROT_NONE);
  frames_store[f_num].read_write_bits = PROT_NONE;
  // increment the number of evictions/replacements.
  ++vm_stats.evictions_count;
}

/**
 * method which prints the resource usage stats collected on console.
 */
void print_resource_usage_stats() {
  printf("%i, %i, %i, %i, %i\n", args.num_pages, args.num_frames, vm_stats.page_faults_count,
      vm_stats.disk_reads_count, vm_stats.disk_writes_count);
}
