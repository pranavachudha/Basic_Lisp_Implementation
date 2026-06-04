#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void print_arr(int arr[], int length){
    for (int i = 0; i < length; i++) {
        printf("%d", arr[i]);
    }
    printf("\n");
}

void arr_filler(int* arr, int length) {
    for (int i = 0; i < length; i++) {
        arr[i] = i + 1;
    }
}

void main(){
    int size = 5;
    int* arr = malloc(sizeof(int)*size);

    int i = 2;

    arr_filler(arr, size);
    print_arr(arr, size);

    memmove(&arr[i], &arr[i+1], sizeof(int)*(size - i - 1));
    size--;
    arr = realloc(arr, size);

    print_arr(arr, size);

    free(arr);
}

