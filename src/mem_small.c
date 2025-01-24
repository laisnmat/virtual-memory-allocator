/******************************************************
 * Copyright Grégory Mounié 2018                      *
 * This code is distributed under the GLPv3+ licence. *
 * Ce code est distribué sous la licence GPLv3+.      *
 ******************************************************/

#include <assert.h>
#include "mem.h"
#include "mem_internals.h"

void *
emalloc_small(unsigned long size)
{
    if(arena.chunkpool == NULL){
        unsigned long remainingSize = mem_realloc_small();
        initLinkedList(remainingSize);
    }
    void * next = *((void **)arena.chunkpool);
    void * currentChunk = mark_memarea_and_get_user_ptr(arena.chunkpool, CHUNKSIZE, SMALL_KIND);
    arena.chunkpool = next; //prend l'adresse du pointeur de tête et va dans le contenu de la case pointée.
    return currentChunk; //renvoie le chunk qu'on a extrait de la liste chainée*/
}

void efree_small(Alloc a) {
    //assert(a.ptr != NULL);
    void *next = arena.chunkpool;
    arena.chunkpool = a.ptr;
    *((void**)arena.chunkpool) = next;
}

void initLinkedList(unsigned long remainingSize){
    void ** currentNode = arena.chunkpool;
    void ** next = NULL;
    for(unsigned long i = 0; i<remainingSize; i += CHUNKSIZE){
        next = (void **)((char*) currentNode + CHUNKSIZE);
        *currentNode = next;
        currentNode = next;
    }
}
