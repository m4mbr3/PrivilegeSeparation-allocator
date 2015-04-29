#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define SIZE_ALIGN (4*sizeof(size_t))
#define SIZE_MASK (-SIZE_ALIGN)
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
    size_t size;
};

/* Add a new block at the top of the heap. Return NULL if things go wrong */
void *
extend_heap(size_t s,
            unsigned int privlev) {
    int times = (s/PAGE_LENGTH)+1;
    //printf ("\nTimes*PAGE_LENGTH = %d, SIZE = %d\n", times*PAGE_LENGTH,(s/PAGE_LENGTH)+1);
    void *chunk = mmap(NULL,
                    times*PAGE_LENGTH,
                    PROT_READ|PROT_WRITE,
                    MAP_PRIVATE|MAP_ANONYMOUS,
                    -1 ,
                    (off_t) 0);
   /* void *chunk;*/
   /* int res = posix_memalign(&chunk, PAGE_LENGTH, times*PAGE_LENGTH); */
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
        page->free->size = (times*PAGE_LENGTH) - s;
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
    return (void *)chunk;
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
                    return (void *)free->ptr;
                }
                else if (free->prev == NULL) {
                    /* first element of the list to be removed */
                    head->free = free->next;
                    while (used->next != NULL) used = used->next;
                    used->next = free;
                    free->prev = used;
                    free->next = NULL;
                    return (void *)free->ptr;
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
                    return (void *)free->ptr;
                }
            }
            else if (free->size > size) {
                /*case where I should reduce the free chunk and create a new one into the used*/
                struct ps_chunk *occ = (struct ps_chunk *) malloc(sizeof(struct ps_chunk));
                occ->ptr    = free->ptr;
                occ->size   = size;
                free->size -= size;
                free->ptr  += size;
                while (used->next != NULL) used = used->next;
                used->next = occ;
                occ->prev  = used;
                occ->next  = NULL;
                return (void *)occ->ptr;
            }
            free = free->next;
        }
        head = head->next;
    }
    return NULL;
}
static int adjust_size(size_t *n) {
    if (*n-1 > PTRDIFF_MAX -SIZE_ALIGN - PAGE_LENGTH) {
        if(*n) {
            return -1;
        }
        else {
            *n = SIZE_ALIGN;
            return 0;
        }
    }
    *n = (*n + SIZE_ALIGN -1) & SIZE_MASK;
    return 0;
}

void *
privsep_malloc (size_t size,
                unsigned int privlev) {
    struct ps_page *page, *last;
    void *ptr;
    size_t s = size;
    //s = align4(size);
    if (privlev < 0) return NULL;
    if (adjust_size(&s) < 0 ) return NULL;

    if(heaps[privlev]){
        ptr = find_block(s, privlev);
        if (ptr == NULL) {
            ptr = extend_heap(s, privlev);
        }
    }
    else {
        ptr = extend_heap (s, privlev);
        if (!ptr)
            return NULL;
    }
    return (void *)ptr;
}

struct ps_page *
get_heap_page(void *p, int *heap_page) {
    int i=0;
    for (i = 0; i < 100; i++){
        if (heaps[i] != NULL) {
            struct ps_page  *page = heaps[i];
            while(page != NULL){
                struct ps_chunk *used = page->used;
                while (used != NULL) {
                    if (used->ptr == p) {
                        *heap_page = i;
                        return page;
                    }
                    used = used->next;
                }
                page = page->next;
            }
        }
    }
    return NULL;
}
void fusion_free_chunk(struct ps_chunk *free_list) {
    /* This function should receive an ordered list */
    if (free_list == NULL) return ;
    while (free_list->next != NULL) {
        struct ps_chunk *next = free_list->next;
        if ((free_list->ptr+free_list->size) == next->ptr ) {
            /*here I should merge the two adjacent blocks freeing next */
            free_list->size += next->size;
            free_list->next == next->next;
            free(next);
        }
        free_list = free_list->next;
    }
}
void free_page (struct ps_page *page, int privlev){
    if(page->used != NULL) return;
    page->free;
    munmap (page->free->ptr, page->size);
    free(page->free);
    if(page->next == NULL && page->prev == NULL) {
        /* Last page */
        free(page);
        heaps[privlev] = NULL;
    }
    if(page->next == NULL) {
        /* Tail element */
        struct ps_page *prev = page->prev;
        prev->next = NULL;
        free(page);
    }
    if(page->prev == NULL) {
        /* First element */
        heaps[privlev] = page->next;
        free(page);
    }
    else {
        /*in the middle */
        struct ps_page *prev = page->prev;
        struct ps_page *next = page->next;
        prev->next = next;
        next->prev = prev;
        free(page);
    }
    return;
}
void
privsep_free(void *p) {
    int num_heap;
    struct ps_page *page = get_heap_page(p, &num_heap);
    if (page == NULL) return;

    struct ps_chunk *free = page->free;
    struct ps_chunk *used = page->used;
    while (used != NULL && used->ptr != p) used = used->next;

    if (used->next == NULL && used->prev == NULL) {
        /* It is the last used block for this page so*/
        while (free != NULL && used->ptr > free->ptr)
            free = free->next;

        if (free == NULL) return;

        free->prev->next = used;
        used->prev = free->prev;
        used->next = free;
        free->prev = used;
        page->used = NULL;
    }
    else if (used->next == NULL) {
        /* It is the tail of the list */
        printf ("It is the tail\n");
    }
    else if (used->prev == NULL) {
        /* It is the head */
        printf ("It is the head\n");
    }
    else {
        /* It is in the middle */
        printf ("It is in the middle\n");
    }
    fusion_free_chunk(page->free);
    if (page->free->size == page->size)
        free_page (page, num_heap);

    return;
}
