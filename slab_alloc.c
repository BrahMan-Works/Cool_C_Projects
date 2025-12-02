/* a basic slab allocator */
#include <stdio.h>
#include <sys/mman.h>
#include <stddef.h>

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

static slab_t* slabList[5] = {0};

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

    slab_t* s = slabList[cI];

    while(s && s->freeList == NULL) s = s->next;

    if(!s)
    {
        s = slabCreate(sizeClasses[cI]);
        if(!s)
        {
            printf("slab creation falied\n");
            return NULL;
        }
        s->next = slabList[cI];
        slabList[cI] = s;
    }

    void* obj = s->freeList;
    s->freeList = *(void**)obj;
    s->used++;

    return obj;
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

    slab_t* s = slabList[cI];
    while(s && !ptrInSlab(s, ptr)) s = s->next;

    if(!s)
    {
        printf("invalid pointer\n");
        return -1;
    }

    *(void**) ptr = s->freeList;
    s->freeList = ptr;
    s->used--;
    
    return 0;
}

int main()
{
    /*
    code to check if this works as it should (it does).

    void* a1 = slabAlloc(20);
    void* a2 = slabAlloc(20);
    void* a3 = slabAlloc(20);

    printf("a1 = %p\n", a1);
    printf("a2 = %p\n", a2);
    printf("a3 = %p\n", a3);

    slabFree(a2, 20);

    void* a4 = slabAlloc(20);
    
    printf("a4 = %p\n", a4);
    
    */
    return 0;
}
