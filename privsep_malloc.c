#include <sys/types.h>
#include <unistd.h>

#define PAGE_LENGTH sysconf(_SC_PAGESIZE)
#define align4(x) (((((x)-1)>>2)<<2)+4)
/* Define the block size since the sizeof will be wrong */
#define BLOCK_SIZE 20

/*block struct */
struct s_block {
    size_t          size;
    struct s_block *next;
    struct s_block *prev;
    int             free;
    void           *ptr;
    /* A pointer to the allocated block */
    char            data[1];
};

/* Definition of t_block */
typedef struct s_block *t_block;

/* Split block according to size. The b block must exist. */
void split_block(t_block b, size_t s){
    t_block new;
    new =(t_block)(b->data + s);
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
t_block extend_heap(t_block last, size_t s){
    int sb;
    t_block b;
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

void *base = NULL;


t_block get_block (void *p) {
    char *tmp;
    tmp = p;
    return (p = tmp -= BLOCK_SIZE);
}
int valid_addr (void *p){
    if (base){
        if (p>base && p<sbrk(0)){
            return (p == (get_block(p))->ptr);
        }
    }
    return (0);
}



t_block find_block(t_block *last, size_t size){
    t_block b = base;
    while (b && !(b->free && b->size >= size)){
        *last = b;
        b = b->next;
    }
    return (b);
}

void *privsep_malloc (size_t size, unsigned int privlev) {
    t_block b, last;
    size_t s;
    s = align4(size);
    if(base){
        last = base;
        b = find_block(&last, s);
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
        base = b;
    }
    return (b->data);
}
t_block fusion (t_block b){
    if (b->next && b->next->free){
        b->size += BLOCK_SIZE + b->next->size;
        b->next = b->next->next;
        if(b->next)
            b->next->prev = b;
    }
    return b;
}
void privsep_free(void *p){
    t_block b;
    if (valid_addr(p))
    {
        b = get_block(p);
        b->free = 1;
        if (b->prev && b->prev->free)
            b = fusion(b->prev);
        if (b->next)
            fusion(b);
        else
        {
            if (b->prev)
                b->prev->next = NULL;
            else {
                base = NULL;
                brk(b);
            }
        }
    }
}
