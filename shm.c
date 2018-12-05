#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct shm_page {
    uint id;
    char *frame;
    int refcnt;
  } shm_pages[64];
} shm_table;

void shminit() {
  int i;
  initlock(&(shm_table.lock), "SHM lock");
  acquire(&(shm_table.lock));
  for (i = 0; i< 64; i++) {
    shm_table.shm_pages[i].id =0;
    shm_table.shm_pages[i].frame =0;
    shm_table.shm_pages[i].refcnt =0;
  }
  release(&(shm_table.lock));
}

int shm_open(int id, char **pointer) {

  acquire(&(shm_table.lock));

  struct proc* current_proc = myproc();

  int i;
  for (i = 0; i < 64; i++) {

    if (shm_table.shm_pages[i].id == id) {

      //CASE 1: ID already exists
      int errorVal = mappages(current_proc->pgdir,
        (void*) PGROUNDUP(current_proc->sz),
        PGSIZE,
        V2P(shm_table.shm_pages[i].frame),
        PTE_W|PTE_U);


      shm_table.shm_pages[i].refcnt++;  //Increment refcnt

      //Return the virtual pointer thru the pointer parameter
      *pointer = (char*) current_proc->sz;

      current_proc->sz = PGROUNDUP(current_proc->sz) + PGSIZE;

      release(&(shm_table.lock));
      return errorVal;	//from mappages(): 0 if ok, -1 if error happened
    }
  }

  //CASE 2: ID does not yet exist
  
  //First, set i to the first available page
  i = 0;
  while (i < 64 && shm_table.shm_pages[i].id != 0) i++;

  //If there is no empty entry in the table
  if (i >= 64) {
    release(&(shm_table.lock));
    return -2;
  }
  
  //Else, we've found an available entry at i, so now just allocate it!
  shm_table.shm_pages[i].id = id;                  //Initialize its id
  shm_table.shm_pages[i].frame = kalloc();         //kalloc a page
  memset(shm_table.shm_pages[i].frame, 0, PGSIZE); //set all its entries to 0
  shm_table.shm_pages[i].refcnt = 1;               //set refcnt to 1


  //Map the page to an available virtual address page
  int errorVal = mappages(current_proc->pgdir,
    (void*) PGROUNDUP(current_proc->sz),
    PGSIZE,
    V2P(shm_table.shm_pages[i].frame),
    PTE_W|PTE_U);

  //Return the virtual pointer thru the pointer parameter
  *pointer = (char*) current_proc->sz;

  release(&(shm_table.lock));
  return errorVal; //from mappages(): 0 if ok, -1 if error happened
}


int shm_close(int id) {

  acquire(&(shm_table.lock));

  int i;
  for (i = 0; i < 64; i++) {

    if (shm_table.shm_pages[i].id == id) {

      //If you're here, then you've found the id in the page table

      if (shm_table.shm_pages[i].refcnt > 0) {  //If refcnt > 0, decrement like normal
        shm_table.shm_pages[i].refcnt--;
      } else {                                  //Else, clear the shm_table entry
        shm_table.shm_pages[i].id = 0;
        shm_table.shm_pages[i].frame = 0;
        shm_table.shm_pages[i].refcnt = 0;
      }
      
      release(&(shm_table.lock));
      return 0;
    }

  }

  //If you're here, then you've tried to close an id which does not exist
  release(&(shm_table.lock));
  return -3;
}
