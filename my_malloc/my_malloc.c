#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include "my_malloc.h"

#define BLOCK_SIZE 40

void* head = NULL;
//When the block size is larger than request size + threhold
//Split the block.
void split_block(t_block blk, size_t size) {
  t_block child;
  child = blk->data + size;
  child->size = blk->size - size - BLOCK_SIZE;
  child->prev = blk;
  child->next = blk->next;
  if(blk->next){
    blk->next->prev = child;
  }
  child->free = 1;
  child->ptr = child->data;
  blk->size = size;
  blk->next = child;
}
//find the first fit block
t_block first_find(t_block* curr, size_t size) {
  t_block temp = head;
  while (temp) {
    if (temp->free && temp->size >= size) {
      break;
    }
    *curr = temp;
    temp = temp->next;
  }
  return temp;
}
//find the best fit block
t_block best_find(t_block* curr, size_t size) {
  t_block temp = NULL;
  t_block tmp = head;
  size_t ans = (size_t)INT_MAX;
  while (tmp) {
    if (tmp->free && tmp->size >= size) {
      if (tmp->size == size) {
	temp = tmp;
	break;
      }
      if (ans > (tmp->size - size)) {
	ans = tmp->size - size;
	temp = tmp;
      }
    }
    *curr = tmp;
    tmp = tmp->next;
  }
  return temp;
}
//find the worst fit block
t_block worst_find(t_block* curr, size_t size) {
  t_block temp = NULL;
  t_block tmp = head;
  size_t ans = (size_t)INT_MIN;
  while (tmp) {
    if (tmp->free && tmp->size >= size) {
      if (ans < (tmp->size - size)) {
	ans = tmp->size - size;
	temp = tmp;
      }
    }
      *curr = tmp;
      tmp = tmp->next;
  }
  return temp;
}
//Extending the heap for larger space
t_block extend_heap(t_block curr, size_t size) {
  t_block temp;
  temp = sbrk(0);
  if (sbrk(BLOCK_SIZE + size) == (void*)-1) {
    return NULL;
  }
  temp->ptr = temp->data;
  temp->size = size;
  temp->next = NULL;
  temp->prev = curr;
  if (curr) {
    curr->next = temp;
  }
  temp->free = 0;
  return temp;
}

void* ff_malloc(size_t size) {
  t_block blk;
  t_block curr;
  //make sure input size is multiple of 8 bytes
  if((size & 0x7) != 0) {
    size = (((size >> 3) + 1) << 3);
  }
  //when head exists
  if (head) {
    curr = head;
    blk = first_find(&curr, size);
    //when find proper block
    if (blk) {
      //when current block is too large, split it.
      if ((blk->size - size) >= (BLOCK_SIZE + 8))
	split_block(blk, size);
      blk->free = 0;
    }
    //Search the free block list without proper one,increament the heap space
    else {
      blk = extend_heap(curr, size);
      if (!blk) {
	return NULL;
      }
    }
  }
  //when the block list is empty, we just create a new space
  else {
    blk = extend_heap(NULL,size);
    if (!blk) {
      return NULL;
    }
    head = blk;
  }
  return blk->data;
}

//Combine next free space together
t_block combineblock(t_block ptr) {
  if (ptr->next) {
    if (ptr->next->free) {
      ptr->size += ptr->next->size + BLOCK_SIZE;
      ptr->next = ptr->next->next;
      if (ptr->next) {
	ptr->next->prev = ptr;
      }
    }
  }
  return ptr;
}
//Move the pointer from data region to start of block
t_block modifyptr(void* ptr) {
  return (ptr -= BLOCK_SIZE);
}
//Decide if pointer for free is valid
int valid(void* ptr) {
  if (head) {
    if (ptr > head && ptr < sbrk(0)) {
      if(ptr == (modifyptr(ptr))->ptr) {
	return 1;
      }
    }
  }
  return 0;
}

void ff_free(void* ptr) {
  if (!valid(ptr)) {
    return;
  }
  t_block temp = modifyptr(ptr); //input ptr points at addr of data region, adjust to block
  temp->free = 1;
  //combine current block with previous block
  if (temp->prev) {
    if (temp->prev->free) {
      temp = combineblock(temp->prev);
    }
  }
  //combine current block with next block
  if (temp->next) {
    combineblock(temp);
  }
  else {
    //currently at the last block
    if (temp->prev) {
      temp->prev->next = NULL;
    }
    else {
      head = NULL;
    }
    brk(temp);
  }
}

void* bf_malloc(size_t size) {
  t_block blk;
  t_block curr;
  if ((size & 0x7) != 0) {
    size = ((size >> 3) + 1) << 3;
  }
  if (head) {
    curr = head;
    blk = best_find(&curr, size);
    //when I find the block, needs to lock this
    if (blk) {
      //When the current block is too large for the size user want
      if ((blk->size - size) >= (BLOCK_SIZE + 8)) {
	split_block(blk, size);
      }
      blk->free = 0;
    }
    else {
      //No proper, need to extend heap.Add lock here
      blk = extend_heap(curr, size);
      if (!blk) {
	return NULL;
      }
    }
  }
  //the list is empty, create the first block in heap
  else {
    blk = extend_heap(NULL, size);
    if (!blk) {
      return NULL;
    }
    head = blk;
  }
  return blk->data;
}

void bf_free(void* ptr) {
  if (!valid(ptr)) {
    return;
  }
  t_block temp = modifyptr(ptr); //input ptr points at addr of data region, adjust to block
    temp->free = 1;
  if (temp->prev && temp->prev->free) {
    temp = combineblock(temp->prev);
  }
  if (temp->next) {
    combineblock(temp);
  }
  else {
    if (temp->prev) {
      temp->prev->next = NULL; //currently at the last block
    }
    else {
      head = NULL;
    }
    brk(temp);
  }
}

void* wf_malloc(size_t size) {
  t_block blk;
  t_block curr;
  if ((size & 0x7) != 0) {
    size = ((size >> 3) + 1) << 3;
  }
  if (head) {
    curr = head;
    blk = worst_find(&curr, size);
    if (blk) {
      //when block is too large, split it
      if ((blk->size - size) >= (BLOCK_SIZE + 8)) {
	split_block(blk, size);
      }
      blk->free = 0;
    }
    else {
      blk = extend_heap(curr, size);
      if (!blk) {
	return NULL;
      }
    }
  }
  else {
    blk = extend_heap(NULL, size);
    if (!blk) {
      return NULL;
    }
    head = blk;
  }
  return blk->data;
}

void wf_free(void* ptr) {
  if (!valid(ptr)) {
    return;
  }
  t_block temp = modifyptr(ptr); //input ptr points at addr of data region
      temp->free = 1;
    if (temp->prev && temp->prev->free) {
      temp = combineblock(temp->prev);
    }
    if (temp->next) {
      combineblock(temp);
    }
    else {
      if (temp->prev) {
	temp->prev->next = NULL; //currently at the last block
      }
      else {
	head = NULL;
      }
      brk(temp);
    }
}
//return whole data segment
unsigned long get_data_segment_size() {
  if (!head) {
    return 0;
  }
  unsigned long res = 0;
  t_block blk = head;
  while (blk) {
    res += BLOCK_SIZE + blk->size;
    blk = blk->next;
  }
  return res;
}
//return free segment space
unsigned long get_data_segment_free_space_size() {
  if (!head) {
    return 0;
  }
  unsigned long res = 0;
  t_block blk = head;
  while (blk) {
    if (blk->free) {
      res += BLOCK_SIZE + blk->size;
    }
    blk = blk->next;
  }
  return res;
}

//for the thread-safe of malloc
pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;
//for the thread-safe of free
pthread_mutex_t lock3 = PTHREAD_MUTEX_INITIALIZER;
void* ts_malloc(size_t size) {
  t_block blk;
  t_block curr;
  if ((size & 0x7) != 0) {
    size = ((size >> 3) + 1) << 3;
  }
  //current block list is not empty
  if (head) {
    curr = head;
    blk = best_find(&curr, size);
    if (blk) {
      //I have found the proper block, to malloc this I need to add a mutex
      pthread_mutex_lock(&lock1);
      //Here I need to check if the b is currently free, cause former thread could malloc this block.
      if (blk->free) {
	if ((blk->size - size) >= BLOCK_SIZE + 8) {
	  split_block(blk, size);
	}
	blk->free = 0;
      }
      else {
	//Former thread has used this block, current thread cannot use it. It needs to malloc again.
	pthread_mutex_unlock(&lock1);
	return ts_malloc(size);
      }
    }
    else {
      //No proper block, I need to extend heap, add a mutex here
      pthread_mutex_lock(&lock2);
      if (curr->next == NULL) {
	//Multiple threads want to extend, first one extends.
	blk = extend_heap(curr, size);
	if (!blk) {
	  return NULL;
	}
      }
      else {
	//Former threads has extend the heap, so current thread is not the tail of list.
	//But do not need to search again, I know former one do not fit and tail one has been
	//extend and used by another thread. So I move forward the pointer and continue extending
	while (curr->next != NULL) {
	  curr = curr->next;
	}
	blk = extend_heap(curr, size);
	if (!blk) {
	  return NULL;
	}
      }
      pthread_mutex_unlock(&lock2);
    }
  }
  else {
    //when the head is NULL, which means I need to extend the heap first. To do this, I need to add a lock to it.
    pthread_mutex_lock(&lock2);
    if (!head) {
      blk = extend_heap(NULL, size);
      if (!blk) {
	return NULL;
      }
      head = blk;
      pthread_mutex_unlock(&lock2);
    }
    else {
      //currently, former threads has created the list, so it is no more empty, curr thread starts again
      pthread_mutex_unlock(&lock2);
      return ts_malloc(size);
    }
  }
  return blk->data;
}

void ts_free(void* ptr) {
  if (!valid(ptr)) {
    return;
  }
  pthread_mutex_lock(&lock3);
  t_block temp = modifyptr(ptr);
  temp->free = 1;
  if (temp->prev && temp->prev->free) {
    temp = combineblock(temp->prev);
  }
  if (temp->next) {
    combineblock(temp);
  }
  else {
    if (temp->prev) {
      temp->prev->next = NULL;
    }
    else {
      head = NULL;
    }
    brk(temp);
  }
  pthread_mutex_unlock(&lock3);
}
