#include <sys/types.h>
#include <unistd.h>

#define PAGE_LENGTH sysconf(_SC_PAGESIZE)
#define align4(x) (((((x)-1)>>2)<<2)+4)
/* Define the block size since the sizeof will be wrong */
#define BLOCK_SIZE 20

void *base = NULL;
void *heaps[100] = {NULL};


/*block struct */
struct ps_chunk {
    size_t           size;
    struct ps_chunk *next;
    struct ps_chunk *prev;
    int              free;
    void            *ptr;
    /* A pointer to the allocated block */
    char            data[1];
};


/* Split block according to size. The b block must exist. */
void
split_block(struct ps_chunk* b,
            size_t s) {
    struct ps_chunk* new;
    new =(struct ps_chunk*)(b->data + s);
    new->size = b->size - s - BLOCK_SIZE;
    new->next = b->next;
    new->prev = b;
    new->free = 1;
    new->ptr = new->data;
    b->size = s;
    b->next = new;
    if (new->next)
        new->next->prev = new;
}

/* Add a new block at the top of the heap. Return NULL if things go wrong */
struct ps_chunk *
extend_heap(struct ps_chunk *last,
            size_t s) {
    int sb;
    struct ps_chunk *b;
    b = sbrk(0);
    sb = (int)sbrk(BLOCK_SIZE + s);
    if (sb < 0)
        return (NULL);
    b->size = s;
    b->next = NULL;
    b->prev = last;
    b->ptr = b->data;
    if (last)
        last->next = b;
    b->free = 0;
    return (b);
}

struct ps_chunk *
get_block (void *p) {
    char *tmp;
    tmp = p;
    return (p = tmp -= BLOCK_SIZE);
}

int
valid_addr (void *p){
    if (base){
        if (p>base && p<sbrk(0)){
            return (p == (get_block(p))->ptr);
        }
    }
    return (0);
}



struct ps_chunk *
find_block(struct ps_chunk **last,
           size_t size,
           unsigned int privlev) {
    struct ps_chunk *b = heaps[privlev];
    while (b && !(b->free && b->size >= size)){
        *last = b;
        b = b->next;
    }
    return (b);
}

void *
privsep_malloc (size_t size,
                      unsigned int privlev) {
    struct ps_chunk *b, *last;
    size_t s;
    s = align4(size);
    if(heaps[privlev]){
        last = heaps[privlev];
        b = find_block(&last, s, privlev);
        if (b) {
            if ((b->size - s) >= (BLOCK_SIZE + 4))
                split_block(b,s);
            b->free = 0;
        }
        else {
            b = extend_heap(last, s);
            if (!b)
                return NULL;
        }
    }
    else {
        b = extend_heap (NULL, s);
        if (!b)
            return (NULL);
        heaps[privlev] = b;
    }
    return (b->data);
}

struct ps_chunk *
fusion (struct ps_chunk *b) {
    if (b->next && b->next->free) {
        b->size += BLOCK_SIZE + b->next->size;
        b->next = b->next->next;
        if(b->next)
            b->next->prev = b;
    }
    return b;
}

void
privsep_free(void *p) {
    struct ps_chunk *b;
    if (valid_addr(p)) {
        b = get_block(p);
        b->free = 1;
        if (b->prev && b->prev->free)
            b = fusion(b->prev);
        if (b->next)
            fusion(b);
        else {
            if (b->prev)
                b->prev->next = NULL;
            else {
                base = NULL;
                brk(b);
            }
        }
    }
}
