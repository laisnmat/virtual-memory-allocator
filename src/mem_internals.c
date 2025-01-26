/******************************************************
 * Copyright Grégory Mounié 2018-2022                 *
 * This code is distributed under the GLPv3+ licence. *
 * Ce code est distribué sous la licence GPLv3+.      *
 ******************************************************/

#include <sys/mman.h>
#include <assert.h>
#include <stdint.h>
#include "mem.h"
#include "mem_internals.h"

unsigned long knuth_mmix_one_round(unsigned long in)
{
    return in * 6364136223846793005UL % 1442695040888963407UL;
}

void *mark_memarea_and_get_user_ptr(void *ptr, unsigned long size, MemKind k)
{
    //les champs du marquage font toujours 8 octets (uint64_t), en 32 comme en 64 bits
    //ptr pointe vers le début du bloc on pointe vers le début pour écrire size dans 8 octets pour écrire dans 8 octets type long long
    uint64_t valeur_magique = knuth_mmix_one_round((unsigned long)ptr) & ~(0b11ULL); //on obtient la valeur magique avec pour 2 premiers bits 00 
    valeur_magique = valeur_magique | k; //on effectue un ou pour remplir les 2 derniers bits avec la taille MemKind
    uint64_t *ptrCurrent = ptr; //on copie l'adresse initial
    *ptrCurrent = size; //dedans on écrit la taille sur 8 octets
    ptrCurrent = ptrCurrent+1; //maintenant l'adresse rajoute décale de 8 octets
    *ptrCurrent = valeur_magique; //on met la magic value dans le contenu qui décale également de 8 octets;
    ptrCurrent = (uint64_t *)ptr+(size/sizeof(uint64_t))-2; //size = taille total du bloc, on prend donc l'adresse du pointeur + la taille total - 16 octets
    *ptrCurrent = valeur_magique;
    ptrCurrent = ptrCurrent+1;
    *ptrCurrent = size;
    ptrCurrent = (uint64_t *)ptr + 2; //on renvoie le pointeur vers l'adresse où l'utilisateur peut écrire (soit 16 octets après l'adresse initiale)
    return ptrCurrent;
}

Alloc
mark_check_and_get_alloc(void *ptr)
{
    Alloc alloc = {};
    uint64_t *p = ptr;
    const unsigned long OFFSET = 16;
    p = (uint64_t *)((char *)p - OFFSET);
    alloc.size = *p;
    p = p + 1;
    uint64_t magic_number = *p;
    alloc.kind = magic_number & 0b11;
    p = (uint64_t *)((char *)ptr+alloc.size-2*OFFSET);
    assert(*p == magic_number); //un débordement de l'utilisateur modifie le marquage de fin
    p = p + 1;
    assert(*p == alloc.size);
    alloc.ptr = (char *)ptr - OFFSET;
    return alloc;
}


unsigned long
mem_realloc_small() {
    assert(arena.chunkpool == 0);
    unsigned long size = (FIRST_ALLOC_SMALL << arena.small_next_exponant);
    arena.chunkpool = mmap(0,
			   size,
			   PROT_READ | PROT_WRITE | PROT_EXEC,
			   MAP_PRIVATE | MAP_ANONYMOUS,
			   -1,
			   0);
    if (arena.chunkpool == MAP_FAILED)
	handle_fatalError("small realloc");
    arena.small_next_exponant++;
    return size;
}

unsigned long
mem_realloc_medium() {
    uint32_t indice = FIRST_ALLOC_MEDIUM_EXPOSANT + arena.medium_next_exponant;
    assert(arena.TZL[indice] == 0);
    unsigned long size = (FIRST_ALLOC_MEDIUM << arena.medium_next_exponant);
    assert( size == (1UL << indice));
    arena.TZL[indice] = mmap(0,
			     size*2, // twice the size to allign
			     PROT_READ | PROT_WRITE | PROT_EXEC,
			     MAP_PRIVATE | MAP_ANONYMOUS,
			     -1,
			     0);
    if (arena.TZL[indice] == MAP_FAILED)
	handle_fatalError("medium realloc");
    // align allocation to a multiple of the size
    // for buddy algo
    arena.TZL[indice] += (size - (((intptr_t)arena.TZL[indice]) % size));
    arena.medium_next_exponant++;
    return size; // lie on allocation size, but never free
}


// used for test in buddy algo
unsigned int
nb_TZL_entries() {
    int nb = 0;
    
    for(int i=0; i < TZL_SIZE; i++)
	if ( arena.TZL[i] )
	    nb ++;

    return nb;
}
