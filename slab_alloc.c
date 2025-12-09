/* slab allocator */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>
#include <pthread.h>

#define PAGE_SIZE 4096
#define IS_SLAB 0x5a5a5a5a5a5a5a5aULL
#define IS_LARGE 0x7f7f7f7f7f7f7f7fULL

static const size_t sizeClasses[] = {16, 32, 64, 128, 256};
static const size_t NUM_CLASSES = 5;

typedef struct slab
{
    uint64_t checker;
    struct slab* next;
    size_t objSize;
    size_t capacity;
    size_t used;
    void* freeList;
} slab_t;

typedef struct large_header
{
    uint64_t checker;
    size_t size;
} large_header_t;

__thread slab_t* threadSlabList[5] = {0};

static int
getClassIndex(size_t size)
{
    for(size_t i = 0; i < NUM_CLASSES; ++i)
    {
        if(size <= sizeClasses[i]) return i;
    }
    return -1;
}

static int
ptrInSlab(slab_t* s, void* ptr)
{
    char* start = sizeof(slab_t) + (char*) s;
    char* end = start + (s->capacity * s->objSize);
    return (((char*)ptr >= start) && ((char*)ptr < end));
}

slab_t* slabCreate(size_t objSize)
{
    slab_t* s = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(s == MAP_FAILED) return NULL;

    s->checker = IS_SLAB;
    s->next = NULL;
    s->objSize = objSize;
    s->capacity = (4096 - sizeof(slab_t)) / objSize;
    s->used = 0;

    char* objStart = (char*)s + sizeof(slab_t);

    char*p = objStart;

    for(size_t i = 0; i < s->capacity - 1; ++i)
    {
        char* next = p + objSize;
        *(void**)p = next;
        p = next;
    }
    *(void**)p = NULL;

    s->freeList = objStart;
    // printf("thread %lu created new slab %p\n", pthread_self(), s);

    return s;
}

void* slabAlloc(size_t size)
{
    int cI = getClassIndex(size);
    if(cI == -1)
    {
        printf("unsupported size\n");
        return NULL;
    }

    slab_t* s = threadSlabList[cI];

    while(s && s->freeList == NULL) s = s->next;

    if(!s)
    {
        s = slabCreate(sizeClasses[cI]);
        if(!s)
        {
            printf("slab creation falied\n");
            return NULL;
        }
        s->next = threadSlabList[cI];
        threadSlabList[cI] = s;
    }

    void* obj = s->freeList;
    s->freeList = *(void**)obj;
    s->used++;

    return obj;
}

void* largeAlloc(size_t size)
{
    size_t total = sizeof(large_header_t) + size;
    size_t pages = (total + 4095) & ~4095;

    large_header_t* h = mmap(NULL, pages, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if(h == MAP_FAILED)
    {
        perror("largeAlloc()");
        return NULL;
    }

    h->checker = IS_LARGE;
    h->size = pages;

    return (void*)(h + sizeof(large_header_t));
}

int slabFree(void* ptr, size_t size)
{
    if(!ptr) return -1;

    int cI = getClassIndex(size);
    if(cI == -1)
    {
        printf("invalid size\n");
        return -1;
    }

    slab_t* prev = NULL;
    slab_t* s = threadSlabList[cI];

    while(s && !ptrInSlab(s, ptr))
    {
        prev = s;
        s = s->next;
    }

    if(!s)
    {
        printf("invalid pointer\n");
        return -1;
    }

    *(void**) ptr = s->freeList;
    s->freeList = ptr;
    s->used--;

    if(s->used == 0)
    {
        if(prev) prev->next = s->next;
        else threadSlabList[cI] = s->next;

        munmap(s, 4096);
    }
    
    return 0;
}

int largeFree(void* ptr, size_t size)
{
    if(!ptr)
    {
        perror("largeFree()");
        return -1;
    }

    large_header_t* h = (large_header_t*)ptr - sizeof(large_header_t);
    munmap(h, h->size);
    return 0;
}

void* myAlloc(size_t size)
{
    if(size > 256) return largeAlloc(size);
    return slabAlloc(size);
}

int myFree(void* ptr)
{
    if(!ptr)
    {
        printf("invalid pointer\n");
        return -1;
    }

    uintptr_t p = (uintptr_t)ptr;
    uintptr_t base = p & ~(PAGE_SIZE - 1);
    void* pageBase = (void*)base;

    uint64_t checker = *((uint64_t*)pageBase);

    if(checker == IS_SLAB)
    {
        slab_t* s = (slab_t*)pageBase;
        size_t objSize = s->objSize;

        slabFree(ptr, objSize);
        return 0;
    }

    if(checker == IS_LARGE)
    {
        large_header_t* h = (large_header_t*)pageBase;
        size_t size = h->size;

        largeFree(ptr, size);
        return 0;
    }

    fprintf(stderr, "myFree: invalid pointer %p\n", ptr);
    abort();
}

int main()
{
    // finally a working, industry grade allocator....     :)
    return 0;
}
