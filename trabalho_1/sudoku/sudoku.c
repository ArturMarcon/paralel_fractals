#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Function to automatically find the best subgrid dimensions for any size
void calculateSubgridDimensions(int size, int *subRows, int *subCols) {
  int root = (int)sqrt(size);
  for (int i = root; i >= 1; i--) {
    if (size % i == 0) {
      *subRows = i;
      *subCols = size / i;
      return;
    }
  }
}

// Function to print the board with dynamic spacing
void printBoard(int **board, int size) {
  for (int row = 0; row < size; row++) {
    for (int col = 0; col < size; col++) {
      if (board[row][col] == 0) {
        printf("  . ");
      } else {
        // Using %3d ensures double-digit numbers up to 25 align perfectly
        printf("%3d ", board[row][col]);
      }
    }
    printf("\n");
  }
}

// Function to check if placing a number is valid
bool isValid(int **board, int size, int subRows, int subCols, int row, int col,
             int num) {
  // Check the row and column
  for (int i = 0; i < size; i++) {
    if (board[row][i] == num || board[i][col] == num) {
      return false;
    }
  }

  // Check the subgrid
  int startRow = row - (row % subRows);
  int startCol = col - (col % subCols);

  for (int i = 0; i < subRows; i++) {
    for (int j = 0; j < subCols; j++) {
      if (board[startRow + i][startCol + j] == num) {
        return false;
      }
    }
  }

  return true;
}

// Function to add random valid clues to the board
void addRandomClues(int **board, int size, int subRows, int subCols,
                    int numClues) {
  int placed = 0;
  int attempts = 0;
  int maxAttempts =
      numClues * 10; // Prevent infinite loops if the board gets too crowded

  while (placed < numClues && attempts < maxAttempts) {
    int row = rand() % size;
    int col = rand() % size;
    int num = (rand() % size) + 1;

    // If the cell is empty and the random number is valid, place it
    if (board[row][col] == 0 &&
        isValid(board, size, subRows, subCols, row, col, num)) {
      board[row][col] = num;
      placed++;
    }
    attempts++;
  }
}

// Function to solve using backtracking
bool solveSudoku(int **board, int size, int subRows, int subCols) {
  for (int row = 0; row < size; row++) {
    for (int col = 0; col < size; col++) {
      if (board[row][col] == 0) {
        // Try numbers 1 through the size of the board
        for (int num = 1; num <= size; num++) {
          if (isValid(board, size, subRows, subCols, row, col, num)) {
            board[row][col] = num;

            if (solveSudoku(board, size, subRows, subCols)) {
              return true;
            }

            board[row][col] = 0; // Backtrack
          }
        }
        return false; // Trigger backtracking if no number fits
      }
    }
  }
  return true;
}

// Updated main to accept command line arguments
int main(int argc, char *argv[]) {
  // CHANGE THIS ONE VARIABLE: Set your target grid size here
  int size = 20;
  unsigned int seed;

  // Check if a seed was provided as a command-line argument
  if (argc > 1) {
    seed = (unsigned int)atoi(argv[1]);
    printf("Using provided seed: %u\n", seed);
  } else {
    seed = (unsigned int)time(NULL);
    printf("No seed provided. Using time-based seed: %u\n", seed);
  }

  // Initialize the random number generator once
  srand(seed);

  int subRows = 0;
  int subCols = 0;

  // Automatically calculate the required subgrid dimensions
  calculateSubgridDimensions(size, &subRows, &subCols);
  printf("Configured for a %dx%d grid with %dx%d subgrids.\n\n", size, size,
         subRows, subCols);

  // Dynamically allocate memory for the 2D array
  int **board = (int **)malloc(size * sizeof(int *));
  for (int i = 0; i < size; i++) {
    board[i] = (int *)malloc(size * sizeof(int));
  }

  // Initialize an empty board
  for (int r = 0; r < size; r++) {
    for (int c = 0; c < size; c++) {
      board[r][c] = 0;
    }
  }

  // Add random clues
  int numClues = size * 2;
  addRandomClues(board, size, subRows, subCols, numClues);

  printf("Original Board with %d random clues:\n", numClues);
  printBoard(board, size);
  printf("\nSolving...\n\n");

  // Start the timer
  clock_t start_time = clock();

  // Run the solver
  bool solved = solveSudoku(board, size, subRows, subCols);

  // Stop the timer
  clock_t end_time = clock();

  // Calculate the elapsed time in seconds
  double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;

  if (solved) {
    printf("Sudoku solved successfully in %.4f seconds:\n", time_spent);
    printBoard(board, size);
  } else {
    printf("No solution exists. Algorithm gave up after %.4f seconds.\n",
           time_spent);
  }

  // Always free dynamically allocated memory
  for (int i = 0; i < size; i++) {
    free(board[i]);
  }
  free(board);

  return 0;
}