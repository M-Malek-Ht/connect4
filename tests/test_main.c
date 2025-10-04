#include "board.h"
#include <assert.h>
#include <stdio.h>

int main(void){
    Board b; board_init(&b);
    int r;
    assert(board_drop(&b, 4, CELL_A, &r) && r==5);
    assert(board_drop(&b, 4, CELL_B, &r) && r==4);
    
    board_init(&b);
    board_drop(&b,1,CELL_A,&r);
    board_drop(&b,1,CELL_A,&r);
    board_drop(&b,1,CELL_A,&r);
    board_drop(&b,1,CELL_A,&r);
    assert(board_is_winning(&b, r, 0, CELL_A));
    printf("ok\n");
    return 0;
}
