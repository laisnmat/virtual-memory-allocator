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
    //ptr pointe vers le début du bloc on pointe vers le début pour écrire size dans 8 octets pour écrire dans 8 octets type long long
    unsigned long valeur_magique = knuth_mmix_one_round(ptr) & ~(0b11); //on obtient la valeur magique avec pour 2 premiers bits 00 
    valeur_magique = valeur_magique | k; //on effectue un ou pour remplir les 2 derniers bits avec la taille MemKind
    unsigned long *ptrCurrent = ptr; //on copie l'adresse initial
    *ptrCurrent = size; //dedans on écrit en unsigned long la taille (donc sur 8 octets taille de unsigned long)
    ptrCurrent = ptrCurrent+1; //maintenant l'adresse rajoute décale de 8 octets
    *ptrCurrent = valeur_magique; //on met la magic value dans le contenu qui décale également de 8 octets;
    ptrCurrent = ptr+(size/sizeof(unsigned long))-2; //size = taille total du bloc, on prend donc l'adresse du pointeur + la taille total - 16 octets
    *ptrCurrent = valeur_magique;
    ptrCurrent = ptrCurrent+1;
    *ptrCurrent = size;
    ptrCurrent = ptr + 2; //on renvoie le poiteur vers l'adresse où l'utilisateur peut écrire (soit 16 octets après l'adresse initiale)
    return ptrCurrent;
}

Alloc
mark_check_and_get_alloc(void *ptr)
{
    Alloc a = {};
    unsigned long *ptrCurrent = ptr;
    ptrCurrent = ptrCurrent - 2; //on décale de 16 octets 
    a.size = *ptrCurrent; //la taille = le contenu qu'on a mit initialement 
    ptrCurrent = ptrCurrent + 1; //pointe vers la magic number
    unsigned long magic_number = *ptrCurrent;
    a.kind = magic_number & 0b11; //assigne la magic number
    ptrCurrent = ptr+(a.size/8)-2; //pointe vers la magic number de fin
    assert(*ptrCurrent == magic_number);  //vérifie que la magic number de début == magic number de fin
    ptrCurrent = ptrCurrent + 1; //pointe vers la taille
    assert(*ptrCurrent == a.size); //vérifie que la taille de début == taille de fin
    ptrCurrent = ptr - 2;
    a.ptr = ptrCurrent;
    return a;
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
