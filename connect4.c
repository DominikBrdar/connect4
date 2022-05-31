#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include <mpi.h>

// Dominik Brdar, Paralelno programiranje, vježba 2, 2022.

#define ROWS    6
#define COLUMNS 7
#define MAX_DEPTH 7

double duration(clock_t start, clock_t end) { // miliseconds
    return ((double)(end - start) * 1000) / CLOCKS_PER_SEC;
}

void ispis(char *board)
{
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLUMNS; j++) {
            printf(" %c", board[i*COLUMNS +j]);
        }
        printf("\n");
    }
    for (int j = 0; j < COLUMNS; j++) 
        printf("-%d", j+1);
    printf("- depth = %d\n", board[ROWS*COLUMNS+3]);
}

int evaluate(char *board, int *offset)
{   
    int i = 0, j = 0, row, col;

    for (int pos = 0; pos < ROWS * COLUMNS; pos++) {
        row = pos / COLUMNS;
        col = pos % COLUMNS;
        for (i = 0; i < 8; i++) {
            if (board[pos] != '.' && pos + 3 * offset[i] > 0 && pos + 3 * offset[i] < ROWS * COLUMNS) {
                for (j = 0; j < 4 && board[pos] == board[pos + j * offset[i]]; j++) {
                    if (i == 0 || i == 1 || i == 7) {
                        if (col == COLUMNS -1) break;
                    }
                    if (i == 1 || i == 2 || i == 3) {
                        if (row == 0) break;
                    }
                    if (i == 3 || i == 4 || i == 5) {
                        if (col == 0) break;
                    }
                    if (i == 5 || i == 6 || i == 7) {
                        if (row == ROWS -1) break;
                    }
                    row = (pos + j * offset[i]) / COLUMNS;
                    col = (pos + j * offset[i]) % COLUMNS;
                }
            }
            if (j == 4) break;
        }
        if (j == 4) {
            if (board[ROWS * COLUMNS+3] % 2) return 0;
            else return 2;
        }
    }
    return 1;
}

int drop_token(char *board, int potez) {
    int rows = ROWS;
    while (rows > 0) {
        if (board[(rows-1) * COLUMNS + potez -1] != '.') 
            rows--; // drop token
        else break;
    }
    if (!rows) return 0; // fail

    board[ROWS * COLUMNS+3]++; // depth
    if (board[ROWS * COLUMNS+3] % 2) // token
        board[(rows-1) * COLUMNS + potez -1] = 'H';
    else
        board[(rows-1) * COLUMNS + potez -1] = 'C'; 
    board[ROWS * COLUMNS] = rows -1;  
    board[ROWS * COLUMNS+1] = potez -1;
    return 1; // done
}

void master(int world_size) 
{
    int num_of_tasks = 0, win = 0, potez, assignee, i;
    float best_move;
    float move_val[COLUMNS];
    MPI_Status status;

    // init boards
    int info = ROWS * COLUMNS;
    int size = info + 5;
    char board[size];
    char cur_board[size];
    /*  last move info (cast to int):   
    *   row = board[ROWS * COLUMNS]         
    *   col = board[ROWS * COLUMNS+1];      
    */
    for (int i = 0; i < info; i++)
        cur_board[i] = '.'; // free slot
    cur_board[info+2] = 0;  // value
    cur_board[info+3] = 0;  // depth
    int depth;

    // Prvi igra human
    ispis(cur_board);
    printf("Unesi sljedeći potez:\n");
    scanf("%d", &potez);
    drop_token(cur_board, potez);
    ispis(cur_board);

    while (!win) {
        clock_t start = clock(); // start timing    
        for (i = 0; i < size; i++) 
            board[i] = cur_board[i];

        // give tasks to slaves 
        for (i = 0; i < COLUMNS; i++) {
            move_val[i] = 0;
            board[info+3] = -1;
            if (drop_token(board, i+1)) {   // iterate possible moves  
                board[size-1] = i;    // (number/potez) of the next move this task is assigned to
                num_of_tasks++;
                assignee = i % (world_size -1) + 1;
                MPI_Send(board, size, MPI_CHAR, assignee, 1, MPI_COMM_WORLD);
                board[(int)board[info] * COLUMNS + i] = '.'; // rollback
            }
            else move_val[i] = -2;
        }

        // manage slaves
        do { // wait for report
            MPI_Recv(board, size, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if (status.MPI_TAG == 2) { // update values, if not leaf -> value = 0
                num_of_tasks--;
                move_val[(int)board[size-1]] += ((float)board[info+2] - 1) / pow(MAX_DEPTH, board[info+3]);
            }
            else if (status.MPI_TAG == 3) { 
                num_of_tasks--;
            }
            else if (status.MPI_TAG == 1) { // got new task, assign it (no checking which slave is faster/slower)
                num_of_tasks++;
                assignee = (++assignee) % (world_size-1) +1;
                MPI_Send(board, size, MPI_CHAR, assignee, 1, MPI_COMM_WORLD);
            }
        } while(num_of_tasks > 0);

        best_move = -1;
        for (int i = 0; i < COLUMNS; i++) {
            if (best_move < move_val[i]) {
                best_move = move_val[i];
                potez = i +1;
            }
        } 
        drop_token(cur_board, potez);
        printf("Computer plays %d\n", potez);
        ispis(cur_board);

        clock_t end = clock();
	    printf("Duration: %fms\n\n", duration(start, end));

        /* 
        for (int j = 0; j < COLUMNS; j++) 
            printf(", %f", move_val[j]);
        printf("\n");
        */
        
        win = move_val[potez-1]; 
        if (win == 1) 
            printf("computer wins\n");
        else if (win == -1)
            printf("You Win!\n");
        else {    
            do {
                printf("Unesi sljedeći potez:\n");
                scanf("%d", &potez);
            } while (!drop_token(cur_board, potez));
            ispis(cur_board);
        }
    }

    // free slaves
    for (int i = 1; i < world_size; i++)
        MPI_Send(board, size, MPI_CHAR, i, 2, MPI_COMM_WORLD);
}

void slave() 
{
    MPI_Status status;
    int info = ROWS * COLUMNS;
    int size = info + 5;
    char board[size];
    char *value = &board[info+2];
    char *depth = &board[info+3];
    
    int offset[8];
    offset[0] = 1;
    offset[1] = -COLUMNS +1;
    offset[2] = -COLUMNS;
    offset[3] = -COLUMNS -1;
    offset[4] = -1;
    offset[5] = COLUMNS -1;
    offset[6] = COLUMNS;
    offset[7] = COLUMNS +1;

    int i;
    do { 
        // wait for task from master, task tag = 1
        MPI_Recv(board, size, MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == 2) break;

        *value = (char)evaluate(board, offset);
        
        //ispis(board);
        // report to master
        if ((*value) -1 || *depth == MAX_DEPTH-1) { // found leaf
            MPI_Send(board, size, MPI_CHAR, 0, 2, MPI_COMM_WORLD);
        } else {
            MPI_Send(board, size, MPI_CHAR, 0, 3, MPI_COMM_WORLD); // inform master of income of new tasks
            for (i = 0; i < COLUMNS; i++) {  // generate all possible moves for next depth level
                if (drop_token(board, i+1)) {
                    MPI_Send(board, size, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
                    board[board[info] * COLUMNS + i] = '.'; 
                    board[info+3]--;
                }
            }
        }
    } while (status.MPI_TAG == 1); // if tag == 2 -> done
}

int main(int argc, char** argv) 
{   
	// Initialize the MPI environment
	MPI_Init(NULL, NULL);

	// Find out rank, size
	int rank;	// id procesa
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	int world_size;	// ukupan broj procesa
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	
    if (rank == 0) {
        printf("Igra 4 u nizu\n\t-> potez se zadaje brojkom stupca\n");
        master(world_size);
    } else 
        slave();
	
	MPI_Finalize();
}