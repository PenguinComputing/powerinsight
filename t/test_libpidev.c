#include "pidev.h"
#include <stdio.h>
#include <stdlib.h>

int main(){
    int result;
    int i;
    reading_t reading;

    printf("opening\n");
    result=pidev_open();
    printf("opened, result=%d\n", result);
    if(result!=PIERR_SUCCESS) { exit(1) ; }

    for(i=1;i<15;i++){
        printf("reading %d\n", i);
        result=pidev_read(i, &reading);
	switch(result) {
	case PIERR_SUCCESS :
            printf("%2d => %7.3f\n",i,reading.reading);
            break ;
        case PIERR_NOTFOUND :
            printf("%2d => NOT FOUND\n",i);
            break ;
        default :
            printf("%2d => ERR %d\n",i,result);
            break ;
        }
    }
    return 0 ;
}
