/* File ListC/listmachine.c
   A unified-stack abstract machine and garbage collector 
   for list-C, a variant of micro-C with cons cells.
   sestoft@itu.dk * 2009-11-17, 2012-02-08

   Compile like this, on ssh.itu.dk say:
      gcc -Wall listmachine.c -o listmachine

   If necessary, force compiler to use 32 bit integers:
      gcc -m32 -Wall listmachine.c -o listmachine

   To execute a program file using this abstract machine, do:
      ./listmachine <programfile> <arg1> <arg2> ...
   To get also a trace of the program execution:
      ./listmachine -trace <programfile> <arg1> <arg2> ...

   This code assumes -- and checks -- that values of type
   int, unsigned int and unsigned int* have size 32 bits.
*/

/*
   Data representation in the stack s[...] and the heap:
    * All integers are tagged with a 1 bit in the least significant
      position, regardless of whether they represent program integers,
      return addresses, array base addresses or old base pointers
      (into the stack).  
    * All heap references are word-aligned, that is, the two least
      significant bits of a heap reference are 00.  
    * Integer constants and code addresses in the program array
      p[...] are not tagged.
   The distinction between integers and references is necessary for 
   the garbage collector to be precise (not conservative).

   The heap consists of 32-bit words, and the heap is divided into 
   blocks.  A block has a one-word header block[0] followed by the 
   block's contents: zero or more words block[i], i=1..n.

   A header has the form ttttttttnnnnnnnnnnnnnnnnnnnnnngg
   where tttttttt is the block tag, all 0 for cons cells
	 nn....nn is the block length (excluding header)
	 gg       is the block's color

   The block color has this meaning:
   gg=00=White: block is dead (after mark, before sweep)
   gg=01=Grey:  block is live, children not marked (during mark)
   gg=10=Black: block is live (after mark, before sweep)
   gg=11=Blue:  block is on the freelist or orphaned

   A block of length zero is an orphan; it cannot be used 
   for data and cannot be on the freelist.  An orphan is 
   created when allocating all but the last word of a free block.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>

typedef unsigned int word;

#define IsInt(v) (((v)&1)==1)
#define Tag(v) (((v)<<1)|1)
#define Untag(v) ((v)>>1)

#define White 0
#define Grey  1
#define Black 2
#define Blue  3

#define BlockTag(hdr) ((hdr)>>24)
#define Length(hdr)   (((hdr)>>2)&0x003FFFFF)
#define Color(hdr)    ((hdr)&3)
#define Paint(hdr, color)  (((hdr)&(~3))|(color))

#define CONSTAG 0

// Heap size in words

#define HEAPSIZE 1000

word * heapFrom;
word * heapTo;
word * afterFrom;
word * afterTo;
word * freelist;

// These numeric instruction codes must agree with ListC/Machine.fs:
// (Use #define because const int does not define a constant in C)

#define CSTI 0
#define ADD 1
#define SUB 2
#define MUL 3
#define DIV 4
#define MOD 5
#define EQ 6
#define LT 7
#define NOT 8
#define DUP 9
#define SWAP 10
#define LDI 11
#define STI 12
#define GETBP 13
#define GETSP 14
#define INCSP 15
#define GOTO 16
#define IFZERO 17
#define IFNZRO 18
#define CALL 19
#define TCALL 20
#define RET 21
#define PRINTI 22
#define PRINTC 23
#define LDARGS 24
#define STOP 25
#define NIL 26
#define CONS 27
#define CAR 28
#define CDR 29
#define SETCAR 30
#define SETCDR 31

#define STACKSIZE 1000
  
// Print the stack machine instruction at p[pc]

void printInstruction(int p[], int pc) {
  switch (p[pc]) {
  case CSTI:   printf("CSTI %d", p[pc+1]); break;
  case ADD:    printf("ADD"); break;
  case SUB:    printf("SUB"); break;
  case MUL:    printf("MUL"); break;
  case DIV:    printf("DIV"); break;
  case MOD:    printf("MOD"); break;
  case EQ:     printf("EQ"); break;
  case LT:     printf("LT"); break;
  case NOT:    printf("NOT"); break;
  case DUP:    printf("DUP"); break;
  case SWAP:   printf("SWAP"); break;
  case LDI:    printf("LDI"); break;
  case STI:    printf("STI"); break;
  case GETBP:  printf("GETBP"); break;
  case GETSP:  printf("GETSP"); break;
  case INCSP:  printf("INCSP %d", p[pc+1]); break;
  case GOTO:   printf("GOTO %d", p[pc+1]); break;
  case IFZERO: printf("IFZERO %d", p[pc+1]); break;
  case IFNZRO: printf("IFNZRO %d", p[pc+1]); break;
  case CALL:   printf("CALL %d %d", p[pc+1], p[pc+2]); break;
  case TCALL:  printf("TCALL %d %d %d", p[pc+1], p[pc+2], p[pc+3]); break;
  case RET:    printf("RET %d", p[pc+1]); break;
  case PRINTI: printf("PRINTI"); break;
  case PRINTC: printf("PRINTC"); break;
  case LDARGS: printf("LDARGS"); break;
  case STOP:   printf("STOP"); break;
  case NIL:    printf("NIL"); break;
  case CONS:   printf("CONS"); break;
  case CAR:    printf("CAR"); break;
  case CDR:    printf("CDR"); break;
  case SETCAR: printf("SETCAR"); break;
  case SETCDR: printf("SETCDR"); break;
  default:     printf("<unknown>"); break; 
  }
}

// Print current stack (marking heap references by #) and current instruction

void printStackAndPc(int s[], int bp, int sp, int p[], int pc) {
  printf("[ ");
  int i;
  for (i=0; i<=sp; i++)
    if (IsInt(s[i]))
      printf("%d ", Untag(s[i]));
    else
      printf("#%d ", s[i]);      
  printf("]");
  printf("{%d:", pc); printInstruction(p, pc); printf("}\n"); 
}

// Read instructions from a file, return array of instructions

int* readfile(char* filename) {
  int capacity = 1, size = 0;
  int *program = (int*)malloc(sizeof(int)*capacity); 
  FILE *inp = fopen(filename, "r");
  int instr;
  while (fscanf(inp, "%d", &instr) == 1) {
    if (size >= capacity) { 
      int* buffer = (int*)malloc(sizeof(int) * 2 * capacity);
      int i;
      for (i=0; i<capacity; i++)
        buffer[i] = program[i];
      free(program);
      program = buffer;
      capacity *= 2;
    }
    program[size++] = instr;
  }
  fclose(inp);
  return program;
}

word* allocate(unsigned int tag, unsigned int length, int s[], int sp);

// The machine: execute the code starting at p[pc] 

int execcode(int p[], int s[], int iargs[], int iargc, int /* boolean */ trace) {
  int bp = -999;        // Base pointer, for local variable access 
  int sp = -1;          // Stack top pointer
  int pc = 0;           // Program counter: next instruction
  for (;;) {
    if (trace) 
      printStackAndPc(s, bp, sp, p, pc);
    switch (p[pc++]) {
    case CSTI:
      s[sp+1] = Tag(p[pc++]); sp++; break;
    case ADD: 
      s[sp-1] = Tag(Untag(s[sp-1]) + Untag(s[sp])); sp--; break;
    case SUB: 
      s[sp-1] = Tag(Untag(s[sp-1]) - Untag(s[sp])); sp--; break;
    case MUL: 
      s[sp-1] = Tag(Untag(s[sp-1]) * Untag(s[sp])); sp--; break;
    case DIV: 
      s[sp-1] = Tag(Untag(s[sp-1]) / Untag(s[sp])); sp--; break;
    case MOD: 
      s[sp-1] = Tag(Untag(s[sp-1]) % Untag(s[sp])); sp--; break;
    case EQ: 
      s[sp-1] = Tag(s[sp-1] == s[sp] ? 1 : 0); sp--; break;
    case LT: 
      s[sp-1] = Tag(s[sp-1] < s[sp] ? 1 : 0); sp--; break;
    case NOT: {
      int v = s[sp];
      s[sp] = Tag((IsInt(v) ? Untag(v) == 0 : v == 0) ? 1 : 0);
    } break;
    case DUP: 
      s[sp+1] = s[sp]; sp++; break;
    case SWAP: 
      { int tmp = s[sp];  s[sp] = s[sp-1];  s[sp-1] = tmp; } break; 
    case LDI:                 // load indirect
      s[sp] = s[Untag(s[sp])]; break;
    case STI:                 // store indirect, keep value on top
      s[Untag(s[sp-1])] = s[sp]; s[sp-1] = s[sp]; sp--; break;
    case GETBP:
      s[sp+1] = Tag(bp); sp++; break;
    case GETSP:
      s[sp+1] = Tag(sp); sp++; break;
    case INCSP:
      sp = sp+p[pc++]; break;
    case GOTO:
      pc = p[pc]; break;
    case IFZERO: { 
      int v = s[sp--];
      pc = (IsInt(v) ? Untag(v) == 0 : v == 0) ? p[pc] : pc+1; 
    } break;
    case IFNZRO: { 
      int v = s[sp--];
      pc = (IsInt(v) ? Untag(v) != 0 : v != 0) ? p[pc] : pc+1; 
    } break;
    case CALL: { 
      int argc = p[pc++];
      int i;
      for (i=0; i<argc; i++)               // Make room for return address
        s[sp-i+2] = s[sp-i];               // and old base pointer
      s[sp-argc+1] = Tag(pc+1); sp++; 
      s[sp-argc+1] = Tag(bp);   sp++; 
      bp = sp+1-argc;
      pc = p[pc]; 
    } break; 
    case TCALL: { 
      int argc = p[pc++];                  // Number of new arguments
      int pop  = p[pc++];                  // Number of variables to discard
      int i;
      for (i=argc-1; i>=0; i--)    // Discard variables
        s[sp-i-pop] = s[sp-i];
      sp = sp - pop; pc = p[pc]; 
    } break; 
    case RET: { 
      int res = s[sp]; 
      sp = sp-p[pc]; bp = Untag(s[--sp]); pc = Untag(s[--sp]); 
      s[sp] = res; 
    } break; 
    case PRINTI:
      printf("%d ", IsInt(s[sp]) ? Untag(s[sp]) : s[sp]); break; 
    case PRINTC:
      printf("%c", Untag(s[sp])); break; 
    case LDARGS: {
      int i;
      for (i=0; i<iargc; i++) // Push commandline arguments
        s[++sp] = Tag(iargs[i]);
    } break;
    case STOP:
      return 0;
    case NIL:    
      s[sp+1] = 0; sp++; break;
    case CONS: {
      word* p = allocate(CONSTAG, 2, s, sp); 
      p[1] = (word)s[sp-1];
      p[2] = (word)s[sp];
      s[sp-1] = (int)p;
      sp--;
    } break;
    case CAR: {
      word* p = (word*)s[sp]; 
      if (p == 0) 
        { printf("Cannot take car of null\n"); return -1; }
      s[sp] = (int)(p[1]);
    } break;
    case CDR: {
      word* p = (word*)s[sp]; 
      if (p == 0) 
        { printf("Cannot take cdr of null\n"); return -1; }
      s[sp] = (int)(p[2]);
    } break;
    case SETCAR: {
      word v = (word)s[sp--];
      word* p = (word*)s[sp]; 
      p[1] = v;
    } break;
    case SETCDR: {
      word v = (word)s[sp--];
      word* p = (word*)s[sp]; 
      p[2] = v;
    } break;
    default:                  
      printf("Illegal instruction %d at address %d\n", p[pc-1], pc-1);
      return -1;
    }
  }
}

// Read program from file, and execute it

int execute(int argc, char** argv, int /* boolean */ trace) {
  int *p = readfile(argv[trace ? 2 : 1]);         // program bytecodes: int[]
  int *s = (int*)malloc(sizeof(int)*STACKSIZE);   // stack: int[]
  int iargc = trace ? argc - 3 : argc - 2;
  int *iargs = (int*)malloc(sizeof(int)*iargc);   // program inputs: int[]
  int i;
  for (i=0; i<iargc; i++)                         // Convert commandline arguments
    iargs[i] = atoi(argv[trace ? i+3 : i+2]);
  // Measure cpu time for executing the program
  struct rusage ru1, ru2;
  getrusage(RUSAGE_SELF, &ru1);
  int res = execcode(p, s, iargs, iargc, trace);  // Execute program proper
  getrusage(RUSAGE_SELF, &ru2);
  struct timeval t1 = ru1.ru_utime, t2 = ru2.ru_utime;
  double runtime = t2.tv_sec-t1.tv_sec+(t2.tv_usec-t1.tv_usec)/1000000.0;
  printf("\nUsed %7.3f cpu seconds\n", runtime);
  return res;
}

// Garbage collection and heap allocation 

word mkheader(unsigned int tag, unsigned int length, unsigned int color) { 
  return (tag << 24) | (length << 2) | color;
}

int inToHeap(word* p) {
  return heapTo <= p && p < afterTo;
}

int inFromHeap(word* p) {
  return heapFrom <= p && p < afterFrom;
}

void initheap() {
  heapFrom = (word*)malloc(sizeof(word)*HEAPSIZE);
  heapTo   = (word*)malloc(sizeof(word)*HEAPSIZE);
  afterFrom = &heapFrom[HEAPSIZE];
  afterTo   = &heapTo[HEAPSIZE];
  // Initially, entire heap is one block on the freelist:
  heapFrom[0] = mkheader(0, HEAPSIZE-1, Blue);
  heapTo[0]   = mkheader(0, HEAPSIZE-1, Blue);
  heapFrom[1] = (word)0;
  heapTo[1]   = (word)0;
  freelist = &heapFrom[0];
}

// Copies the word from the from list to the to list and
// returns the new address in the to list
word * copy(word * block) {
  // First examine if the block has been copied
  if (block[1] != 0 && !IsInt(block[1]) && inToHeap(block[1])) {
    return block[1];
  } else {
    word * address = freelist;

    // Store each element on the to heap
    int n, length = Length(block[0]);
    for (n = 0; n <= length; n++) {
      freelist[n] = block[n];
    }
    // Set the forwarding pointer
    block[1] = freelist;

    // Increment the freelist
    freelist += length + 1;

    // Return the new address
    return address;
  }
}

void copyFromTo(int s[], int sp) {
  // Use the freelist as the allocation pointer in the to heap
  freelist = heapTo;
  // Update references from the stack
  int n;
  for (n = 0; n < sp; n++) {
    if (!IsInt(s[n]) && s[n] != 0) {
      s[n] = copy(s[n]);
    }
  }

  // Update references from the to heap
  word * ptr = heapTo;
  while (ptr != freelist) {
    int n, length = Length(ptr[0]);
    for (n = 1; n <= length; n++) {
      if (!IsInt(ptr[n]) && ptr[n] != 0 && inFromHeap(ptr[n])) {
	ptr[n] = copy(ptr[n]);
      }
    }
    ptr += length + 1;
  }

  // Set the free space of the freelist
  freelist[0] = mkheader(0, afterTo - freelist - 1, Blue);

  // Swap the heaps
  word * tmp = heapFrom;
  heapFrom = heapTo;
  heapTo = tmp;
  tmp = afterFrom;
  afterFrom = afterTo;
  afterTo = tmp;
}

word* allocate(unsigned int tag, unsigned int length,
		int s[], int sp) {
  int attempt = 1;
  do {
    word* newBlock = freelist;
    freelist += length + 1;
    if (freelist <= afterFrom) {
      newBlock[0] = mkheader(tag, length, White);
      return newBlock;
    }
    // No free space, do a garbage collection and try again
    if (attempt==1)
      copyFromTo(s, sp);
  } while (attempt++ == 1);
  printf("Out of memory\n");
  exit(1);
}

// Read code from file and execute it
int main(int argc, char** argv) {
  if (sizeof(word)!=4 || sizeof(word*)!=4 || sizeof(int)!=4) {
    printf("Size of word, word* or int is not 32 bit, cannot run\n");
    return -1;
  } else if (argc < 2) {
    printf("Usage: listmachine [-trace] <programfile> <arg1> ...\n");
    return -1;
  } else {
    int trace = argc >= 3 && 0==strncmp(argv[1], "-trace", 7);
    initheap();
    return execute(argc, argv, trace);
  }
}
