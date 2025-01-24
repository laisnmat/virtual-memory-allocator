/******************************************************
 * Copyright Grégory Mounié 2018                      *
 * This code is distributed under the GLPv3+ licence. *
 * Ce code est distribué sous la licence GPLv3+.      *
 ******************************************************/

#include <stdint.h>
#include <assert.h>
#include "mem.h"
#include "mem_internals.h"


unsigned int puiss2(unsigned long size) {
    unsigned int p=0;
    size = size -1; // allocation start in 0
    while(size) {  // get the largest bit
	p++;
	size >>= 1;
    }
    if (size > (1 << p))
	p++;
    return p;
}


void *
emalloc_medium(unsigned long size)
{
    assert(size < LARGEALLOC);
    assert(size > SMALLALLOC);
    //regarder l'indice du tableau, elle peut être vide 
    unsigned long index = puiss2(size+32); 
    void **memory = arena.TZL[index];
    //regarde si la mémoire est vide
    if(memory == NULL){
        memory = search_upper_memory(index);
    }
    //une fois qu'on a obtenu la case mémoire, on l'alloue.
    void* next = *memory;
    unsigned long upperSize = 1<<index; //on arrondi à la puissance de 2 supérieur la size
    void* currentChunk = mark_memarea_and_get_user_ptr(memory, upperSize, MEDIUM_KIND); //le marquage est prit en compte dans le emalloc_medium mais pas dans le mark_memarea;
    arena.TZL[index] = next;
    return currentChunk;
}

void **search_upper_memory(unsigned long init_index){
    unsigned long upper_limit = FIRST_ALLOC_MEDIUM_EXPOSANT + arena.medium_next_exponant;
    unsigned long current_index;
    for (current_index = init_index+1; current_index < upper_limit && arena.TZL[current_index] == NULL; current_index++); //tant qu'on ne trouve pas une case libre ou qu'on n'a pas atteint la limite maximum 
    //si on a atteint la limite maximum on realloc
    if(current_index >= upper_limit){
        mem_realloc_medium(); //dans ce cas là current_index ==  FIRST_ALLOC_MEDIUM_EXPOSANT + arena.medium_next_exponant qui est l'index où on realloc
    }
    //maintenant qu'on a notre case mémoire, on va vouloir la diviser jusqu'à atteindre notre size initial.
    divide_memory(init_index, current_index);
    return arena.TZL[init_index]; //on renvoie la case mémoire à l'adresse d'index initial maintenant qu'elle est remplie grâce au divide
}

void divide_memory(unsigned long init_index, unsigned long current_index){
    do {
        void** memory = arena.TZL[current_index]; //on prend notre poiteur de tête, qui est notre case mémoire
        unsigned long buddy_exposant = current_index-1;
        void* buddy = ((char *) memory + (1<<buddy_exposant)); //on divise en 2 la case mémoire (fonctionnement buddy, on prend l'adresse mémoire initial et on rajoute 1 bit à la position k-1)
        arena.TZL[current_index] = *memory; //on pointe vers la case suivante de la liste chaînée (potentiellement nulle)
        *memory = buddy; //la case mémoire de memory pointe maintenant vers buddy (liste chaînée)
        arena.TZL[--current_index] = memory; //et on stock la case mémoire dans l'indice inférieur
    } while(current_index > init_index); // on s'arrête de diviser jusqu'à ce qu'on atteigne l'index initial (donc la taille initial)
}

void efree_medium(Alloc a) {
    unsigned long index;
    unsigned long upper_limit = FIRST_ALLOC_MEDIUM_EXPOSANT + arena.medium_next_exponant;
    //parcourir index changer la condition
    void** currentMem = a.ptr;
    *currentMem = NULL;
    void** previousMem = currentMem;
    for(index = puiss2(a.size); index < upper_limit && (currentMem = findBuddy(index, currentMem)) != NULL; index++){
        previousMem = currentMem;
    }
    //sortie de boucle: soit on a pas trouvé de buddy (ou du moins on a suffisamment fusionné pour ne plus en trouver), soit on a atteint la limite d'indexation.
    void* next = arena.TZL[(index == upper_limit) ? --index : index]; //on a dépassé la limite (on ne peut donc plus fusionner), on place donc notre bloc fusionné dans la case mémoire présente à la limite //sinon on a pas trouvé de buddy pour l'index actuel, donc on place notre bloc a cette adresse
    *previousMem = next; //la précédente tête de liste devient la suite de notre nouvelle tête de liste
    arena.TZL[index] = previousMem; //on place notre bloc mémoire
}

void* findBuddy(unsigned long index, void* memSearchBuddy){
    void** currentMem = arena.TZL[index]; //on prend la case mémoire initial
    void** previousMem = NULL;
    unsigned long size = 1<<index;
    while(currentMem != NULL && ((uintptr_t) currentMem ^ size) != (uintptr_t) memSearchBuddy){
        previousMem = currentMem; //on garde le précédent pour supprimer de la liste chaînée le buddy si le suivant s'avère être le buddy;
        currentMem = *currentMem; //on passe au suivant;
    }
    //soit on a trouvé le buddy, soit on a currentMem == NULL
    //on veut renvoyer l'adresse mémoire initial, donc on renvoie la plus petite adresse entre le buddy et la mémoire de base
    if(currentMem != NULL){ //alors on a trouvé un buddy, on enlève le buddy de la liste chaînée
        if(previousMem == NULL)
            arena.TZL[index] = *currentMem; //on met l'élément suivant
        else 
            *previousMem = *currentMem; //on fait maintenant pointer previousMem par le suivant du buddy, dans la liste chaînée 
        return (uintptr_t)currentMem < (uintptr_t)memSearchBuddy ? currentMem : memSearchBuddy; //on renvoie la plus petite valeur pour renvoyer l'adresse
    }
    //si currentMem == NULL, alors on a pas trouvé de buddy, on renvoie donc NULL
    return NULL;
}