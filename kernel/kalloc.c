// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  struct run *freelist;
} ksmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ksmem.lock, "ksupermem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  int pssz = PGSIZE, superpg = 0;
  for(; p + pssz <= (char*)pa_end; p += pssz){
    if((uint64)p >= PAGE_BOUNDARY){
      pssz = SUPERPGSIZE;
      if(!superpg){
        superpg = 1;
        p = (char*)SUPERPGROUNDUP((uint64)p);
      }
    }
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP){
    printf("kfree: pa=%p\n", pa);
    panic("kfree");
  }
  int pgsz = PGSIZE, superpg = 0;
  if((uint64)pa >= PAGE_BOUNDARY){
    if((uint64)pa % SUPERPGSIZE != 0)
      panic("ksfree");
    superpg = 1;
    pgsz = SUPERPGSIZE;
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, pgsz);

  r = (struct run*)pa;
  if(superpg == 1){

    acquire(&ksmem.lock);
    r->next = ksmem.freelist;
    ksmem.freelist = r;
    release(&ksmem.lock);
  } else {

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  // printf("kalloc: %p\n", r);
  return (void*)r;
}

void *
ksalloc(void)
{
  struct run *r;
  acquire(&ksmem.lock);
  r = ksmem.freelist;
  if(r)
    ksmem.freelist = r->next;
  release(&ksmem.lock);
  if(r)
    memset((char*)r, 5, SUPERPGSIZE); // fill with junk
  // printf("ksalloc: %p\n", r);
  return (void*)r;
}