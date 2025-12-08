/* a basic slab allocator */
#include <stdio.h>
#include <sys/mman.h>
#include <stddef.h>
#include <pthread.h>

static const size_t sizeClasses[] = {16, 32, 64, 128, 256};
static const size_t NUM_CLASSES = 5;

typedef struct slab
{
    struct slab* next;
    size_t objSize;
    size_t capacity;
    size_t used;
    void* freeList;
} slab_t;

typedef struct large_header
{
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

int myFree(void* ptr, size_t size)
{
    if(size > 256) return largeFree(ptr, size);
    return slabFree(ptr, size);
}

/*
void* worker(void* arg)
{
    for(int i = 0; i < 100000; ++i)
    {
        void* p = myAlloc(20);
        slabFree(p, 20);
    }

    return NULL;
}
*/

int main()
{
    /*
    code to check if this works as it should (it does).  

    pthread_t threads[4];

    for(int i = 0; i < 4; ++i)
    {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    for(int i = 0; i < 4; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    */
    return 0;
}
