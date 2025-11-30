/* basic bump allocator */
#include <stdio.h>
#include <sys/mman.h>

static char* arena = NULL;
static size_t arenaSize = 0;
static size_t offset = 0;

int bumpInit(size_t size)
{
    arena = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(arena == MAP_FAILED)
    {
        perror("bump_init()");
        return 1;
    }
    arenaSize = size;
    offset = 0;
    return 0;
}

static size_t align(size_t x)
{
    const size_t alignTo = 8;
    size_t remainder = x % alignTo;
    return remainder == 0 ? x : x + (alignTo - remainder);
}

void* bumpAlloc(size_t size)
{
    size = align(size);

    if(offset + size > arenaSize)
    {
        printf("Allocation Failed\n");
        return NULL;
    }

    void* ptr = arena + offset;
    offset += size;
    return ptr;
}

void bumpReset()
{
    offset = 0;
}

int main()
{
    bumpInit(1024 * 1024);
    int* a = bumpAlloc(sizeof(int));
    int* b = bumpAlloc(sizeof(int) * 10);
    char* c = bumpAlloc(13);

    *a = 2; *b = 6;
    *c = 'A';
    
    printf("The addresses of a, b and c are: %p\t%p\t%p\n", a, b, c);
    printf("The values of a, b and c are: %d\t%d\t%c\n", *a, *b, *c);

    bumpReset();

    int* A = bumpAlloc(sizeof(int));
    int* B = bumpAlloc(sizeof(int) * 10);
    char* C = bumpAlloc(13);

    *A = 17; *B = 2;
    *C = 'A';
    
    printf("The addresses of A, B and C are: %p\t%p\t%p\n", A, B, C);
    printf("The values of A, B and C are: %d\t%d\t%c\n", *A, *B, *C);

    return 0;
}













