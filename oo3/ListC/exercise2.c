#include "listmachine.c"


/***************************
 ***************  Exercise 2
****************************/
void mark(word* block) {
  // Paint grey to signal unfinished business
  block[0] = Paint(block[0], Grey);
  int n;
  int length = Length(block[0]);

  for (n = 1; n <= length; n++) {
    word * child = block[n];
    if (inHeap(child) && Color(child[0]) != Grey) {
      mark(child);
    }
  }
  // Paint Black to signal we're done
  block[0] = Paint(block[0], Black);
}

void markPhase(int s[], int sp) {
  printf("\nmarking ...\n");
  
  int n;
  for (n = 0; n < sp; n++) {
    if (!IsInt(s[n]) && s[n] != 0) {
      mark(s[n]);
    }
  }
}


// Sweeps a block of heap memory and returns the size of the block
void sweepBlock(word * block) {
  int color = Color(block[0]);
        
  if (color == White) {
    // Set the color to blue
    block[0] = Paint(block[0], Blue);
    // Set the next element to point at the freelist

    block[1] = freelist;
    // Set the freelist to point to the current free block
    freelist = block;
  } else if (color == Black) {
    block[0] = Paint(block[0], White);
  }
}

void sweepPhase() {
  printf("\nsweeping ...\n");
  word * block = heap;
  while(inHeap(block)) {
    int length = Length(block[0]);
    // Avoid zero length orphans
    if (length > 0) {
      sweepBlock(block);
    }
    block += length + 1;
  }
}
