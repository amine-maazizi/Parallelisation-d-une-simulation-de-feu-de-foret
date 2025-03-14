# Fichier est 2 pages max

- Ctrl \ (force l'arret)

- résumé modifications
- explications problèmes
- validation (code sequentiel = code parallel | réponse (indication) : verifier qu'il s'arette à l'iteration x)
- speedup
- explications
- courbe de speed up et ideal

- est-ce que ca serait pertinent de faire un appel send bloquant... (buffer d'envoie)
4 branches ou 4 répétoires (README.md)

- enlever le wait pour ne pas fausser les mesures

- seg fault (core dumped):
    - Make DEBUG=yes all (compile une version avec du débogage)
    - gdb (run, l list, n next, p [x] print, b breakpoint)

## Partie 1

- Taille physique et coeurs ...

-...

- Validation: prennent meme steps num que la sequentiel

- PROBLEM: 

## Partie 2
- Modifications:
    1. Travail sur la fonction main, ou j'ai séparer la boucle principale en une partie affichage gerer par le processus 0 et une partie calcul gerer par le reste des processus
    2. processus 0 s'occuper de l'affichage, il initialize le Display et s'occupe de la partie affichage de la boucle, le reste des processus s'occupe de la partie calculatoire du probleme
    3. Un premier echange initiale se fait, ou processus 1 envoie de maniere bloquante la geometry de la simulation qui est recu de maniere bloquante par le 0. certes la donnée de la geometry est accessible via les arguments de la command line pour tous processus, mais cet implementation gere un cas ou la geometry du problem est variable (pas dans ce projet)
    4. ensuite dans notre boucle il y a un echange, reception bloquante des carte de la gvegetation du feux et aussi d'un boolean `running` qui dicte si la simulation du coté `update` est encore en cours.
- problemes rencontré:
    0. L'implémentation initiale que j'ai fais ne separere pas les boucles. C'était 1 boucle et à l'inteireur des boucles on sépare affichage et calcul. mais je m'étati vite rnedue compte de l'inéficacité de cet derniere vue que j'initialiser deux fenetre et de plus...  
    1. premier probleme rencontré est le mismatch des data type lors des envoie, mais c'était vite fixé.
    2. Un autre probleme plus majeur été l'usage de recepetion non bloquante lors de l'initalisation de la geometry dans le processus 0. qui donc n'attende pas la reception de cet derniere et passer au lignes qui suivait qui eu initialize les taille des maps par un valeur aléoitroie ce qui causé des problemss
    3. Lors du lancement de la simulation avec 2 threads ca fin causé une seg fault `Primary job  terminated normally, but 1 process returned` J'ai su que la cause été du au fait que le rank 0 quité immédiatement le program après le signal est donc l'aisse le rank 1 avec des envoie non récéptionné, ce qui perturbe MPI_Finnalize(). Pour la fixé au lieu de sortir de la boucle d'affichage après SDL_QUIT 
- Résultats:
    1. -n 2 Temps global moyen pris par iteration en temps: 0.00145327 seconds
    2. Sequentiel: Temps global moyen pris par iteration en temps: 0.00121068 seconds
    La surcharge de communication et de synchronisation entre les deux threads dans l'exécution parallèle dépasse le gain de temps obtenu en divisant la charge de travail, rendant l'exécution séquentielle plus rapide.