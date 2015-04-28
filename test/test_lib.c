#include <libprivsep_malloc.h>
#include <stdio.h>
#include <unistd.h>

int main (void){
    int i = 0;
    for(i=1;i<95; i++) {
        int *p = privsep_malloc(400, 1);
        int *c = privsep_malloc(400, 1);
        int *p1 = privsep_malloc(sizeof(int)*30, 2);
        int *p2 = privsep_malloc(sizeof(char),2);
        if (p != NULL && c != NULL){
            *p = i;
            *c = *p + 10;
            printf("%p = %d \n%p = %d\n",(int *)p,*((int *) p),(int *)c, *((int *)c));
            *p1=*c-10;
            *p2=*p1+3;
            printf("%p = %d \n%p = %d\n", (int *)p1,*((int *)p1),(int *)p2, *((int *)p2));
        }
        return 1;
       // privsep_free(p);
    }
}
