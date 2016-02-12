#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef struct t_block_t* t_block;
struct t_block_t {
  size_t size; //8
  t_block prev; //8
  t_block next; //8
  int free; //4
  int padding; //4
  void* ptr; //8
  char data[1];
};
//When no proper free block, create a block on the heap
t_block extend_heap(t_block curr, size_t size);
//When block is larger than a threhold, split it up.
void split_block(t_block blk, size_t size);
//First fit policy
void* ff_malloc(size_t size);

t_block first_find(t_block* curr, size_t size);

void ff_free(void* ptr);
//best fit malloc version
void* bf_malloc(size_t size);

t_block best_find(t_block* curr, size_t size);

void bf_free(void* ptr);
//worst fit malloc version
void* wf_malloc(size_t size);

t_block worst_find(t_block* curr, size_t size);

void wf_free(void* ptr);
//combine small pieces free blocks together
t_block combineblock(t_block ptr);
//input a data region pointer, move it to the start of metadata
t_block modifyptr(void* ptr);
//decide if the address for free is valid or not
int valid(void* ptr);
//thread_safe policy functions
void* ts_malloc(size_t size);

void ts_free(void* ptr);
