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

int main()
{
    /*
    code to check if this works as it should (it does).
    
    for(size_t i = 0; i < NUM_CLASSES; ++i)
    {
        printf("slab for %zu bytes:\n", sizeClasses[i]);
        slab_t* s = slabCreate(sizeClasses[i]);
        if(s == NULL)
        {
            perror("slabCreate()");
            return 1;
        }

        printf("slab: %p\ncapacity: %zu\n", s, s->capacity);
        printf("free list: %p %p %p\n\n", s->freeList, s->freeList + (s->objSize), s->freeList + (2*s->objSize));
    }

    */
    return 0;
}
