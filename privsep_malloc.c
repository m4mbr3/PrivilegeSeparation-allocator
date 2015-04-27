#include <sys/types.h>
#include <sys/mman.h>
#include <stdlib.h>
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
    void            *ptr;
};

/* Page struct with list of empty and used chunks */
struct ps_page {
    struct ps_chunk *free;
    struct ps_chunk *used;
    struct ps_page *next;
    struct ps_page *prev;
};

/* Split block according to size. The b block must exist. */
void
split_block(struct ps_chunk* b,
            size_t s) {
    struct ps_chunk* new;
    //new =(struct ps_chunk*)(b->data + s);
    new->size = b->size - s - BLOCK_SIZE;
    new->next = b->next;
    new->prev = b;
    //new->ptr = new->data;
    b->size = s;
    b->next = new;
    if (new->next)
        new->next->prev = new;
}

/* Add a new block at the top of the heap. Return NULL if things go wrong */
void *
extend_heap(size_t s,
            unsigned int privlev) {

    int times = (s%PAGE_LENGTH)+1;
    void *chunk = mmap(NULL,
                    times*PAGE_LENGTH,
                    PROT_READ|PROT_WRITE,
                    MAP_PRIVATE|MAP_ANONYMOUS,
                    -1 ,
                    (off_t) 0);
    if (chunk == MAP_FAILED) return NULL;
    if (heaps[privlev] != NULL) {
        /*scorro pagine e aggiungo una pagina*/
        struct ps_page *page = heaps[privlev];
        while (page->next != NULL) page = page->next;
        /* create the new page for this privilege level */
        page->next = (struct ps_page *) malloc(sizeof(struct ps_page));
        /*setting prev pointer */
        page->next->prev = page;
        /*moving to the new created page */
        page = page->next;
        /*creation of free first chunk */
        page->free = (struct ps_chunk *)malloc (sizeof(struct ps_chunk));
        /*creation of used first chunk */
        page->used = (struct ps_chunk *)malloc (sizeof(struct ps_chunk));
        /*initialization of first used chunk */
        page->used->size = s;
        page->used->next = NULL;
        page->used->prev = NULL;
        page->used->ptr = chunk;
        /*initialization of second used chunk */
        page->free->size = PAGE_LENGTH - s;
        page->free->next = NULL;
        page->free->prev = NULL;
        page->free->ptr = chunk+s;
        /*setting next and prev pointer to null */
        page->next = NULL;

    }
    else {
        /*genero la prima pagina*/
        struct ps_page *page = (struct ps_page *) malloc(sizeof(struct ps_page));
        /*creation of free first chunk */
        page->free = (struct ps_chunk *)malloc (sizeof(struct ps_chunk));
        /*creation of used first chunk */
        page->used = (struct ps_chunk *)malloc (sizeof(struct ps_chunk));
        /*initialization of first used chunk */
        page->used->size = s;
        page->used->next = NULL;
        page->used->prev = NULL;
        page->used->ptr = chunk;
        /*initialization of second used chunk */
        page->free->size = PAGE_LENGTH - s;
        page->free->next = NULL;
        page->free->prev = NULL;
        page->free->ptr = chunk+s;
        /*setting next and prev pointer to null */
        page->next = NULL;
        page->prev = NULL;
        /*stick new page to head of the heap for the requeste privlev */
        heaps[privlev] = page;
    }
    return chunk;
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

void *
find_block(size_t size,
           unsigned int privlev) {

    struct ps_page *head = heaps[privlev];

    while (head != NULL) {
        struct ps_chunk *free = head->free;
        struct ps_chunk *used = head->used;
        while (free != NULL) {
            if (free->size == size) {
                /*case where I should move the whole chunk*/
                if (free->prev == NULL && free->next == NULL){
                    /* case free is head of the list */
                    head->free = NULL;
                    while (used->next != NULL) used = used->next;
                    used->next = free;
                    free->prev = used;
                    free->next = NULL;
                    return free->ptr;
                }
                else if (free->prev == NULL) {
                    /* first element of the list to be removed */
                    head->free = free->next;
                    while (used->next != NULL) used = used->next;
                    used->next = free;
                    free->prev = used;
                    free->next = NULL;
                    return free->ptr;
                }
                else {
                    /* element in the middle of the list */
                    struct ps_chunk *pre_free = free->prev;
                    pre_free->next = free->next;
                    free->next->prev = pre_free;
                    while(used->next != NULL) used = used->next;
                    used->next = free;
                    free->prev = used;
                    free->next = NULL;
                    return free->ptr;
                }
            }
            else if (free->size > size) {
                /*case where I should reduce the free chunk and create a new one into the used*/
                struct ps_chunk *occ = (struct ps_chunk *) malloc(sizeof(struct ps_chunk*));
                occ->ptr    = free->ptr;
                occ->size   = size;
                free->size -= size;
                free->ptr  += size;
                while (used->next != NULL) used = used->next;
                used->next = occ;
                occ->prev  = used;
                occ->next  = NULL;
                return occ->ptr;
            }
            free = free->next;
        }
        head = head->next;
    }
    return NULL;
}

void *
privsep_malloc (size_t size,
                unsigned int privlev) {
    struct ps_page *page, *last;
    void *ptr;
    size_t s;
    s = align4(size);

    if(heaps[privlev]){
        void *ptr = find_block(s, privlev);
        if (ptr == NULL) {
            ptr = extend_heap(s, privlev);
        }
    }
    else {
        ptr = extend_heap (s, privlev);
        if (!ptr)
            return NULL;
    }
    return ptr;
}

struct ps_chunk *
fusion (struct ps_chunk *b) {
    if (b->next) {
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
        if (b->prev)
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
