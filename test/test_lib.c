#include <libprivsep_malloc.h>
#include <stdio.h>
int main (void){
    int i = 0;
    for(i=0;i<1000; i++) {
        int *p = (int *)privsep_malloc(sizeof(int)*i, 1);
        if (p != NULL){
            *p = i;
            printf("%d", *p);
        }
        privsep_free(p);
    }
}
