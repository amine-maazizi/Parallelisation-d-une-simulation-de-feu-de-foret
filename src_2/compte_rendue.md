# Fichier est 2 pages max

- Ctrl \ (force l'arret)

- résumé modifications
- explications problèmes
- validation (code sequentiel = code parallel | réponse : verifier qu'il s'arette à l'iteration x)
- speedup
- explications
- courbe de speed up et ideal

- est-ce que ca serait pertinent de faire un appel send bloquant... (buffer d'envoie)
4 branches ou 4 répétoires (README.md)

- enlever le wait pour ne pas fausser les mesures

- seg fault (core dumped):
    - Make DEBUG=yes all (compile une version avec du débogage)
    - gdb (run, l list, n next, p [x] print, b breakpoint)

## Partie 1 : Parallélisation avec OpenMP

### Modifications apportées
1. **Création d'un vecteur `fire_keys`**  
   Ce vecteur est utilisé pour stocker les clés de `m_fire_front`.

2. **Ajout de `#pragma omp parallel for`**  
   Cette directive parallélise la boucle sur les indices de `fire_keys`.

3. **Utilisation de `#pragma omp critical`**  
   Cette directive sécurise les mises à jour de `m_fire_map` et de `next_front`, évitant ainsi les race conditions.

### Validation de la parallélisation
- Pour s'assurer que la simulation parallel et sequentiel sont identique on s'est basé sur 2 critères
1. **Indicateur initial :**   Le nombre de pas de simulation est identique entre la version séquentielle et la version parallèle (544 pas).
2. **Analyse détaillée :**  Nous avons constaté que le fait d’avoir 544 pas identiques ne garantit pas que les deux simulations évoluent de manière identique. En effet, nous avons comparé les valeurs de `m_fire_map` et `m_vegetation_map`, qui reflètent l’évolution détaillée du feu et de la végétation.  
Une parallélisation mal implémentée (par exemple, en cas de conditions de course ou de tirages pseudo-aléatoires désynchronisés) peut modifier ces cartes, car des mises à jour concurrentes ou désordonnées altèrent la propagation du feu par rapport à l'ordre strictement séquentiel.  
Pour comparer ces cartes de façon efficace (l'affichage complet étant impossible à cause du volume de données), nous avons suivi la suggestion d'un LLM : utiliser un **hash SHA-1**. Ce hash est calculé sur les cartes sérialisées dans un ordre fixe, et l'identité des hashes confirme, de manière rapide et fiable, l'équivalence des simulations.

- **Valeurs SHA-1 obtenues :**

  - **Code parallèle :**
    ```
    SHA-1 à t=1:   e351b2af36ea88cdf1f81379a92bfd56483e1b8a
    SHA-1 à t=2:   02455e2cc0e2a18e4029740356af555a14efff66
    SHA-1 à t=3:   5947dd2d99ababef3444ffe8fbf0d5fd56c8ee81
    SHA-1 à t=4:   ab386c4a01054d0b9683236bd0a92214141c427b
    SHA-1 à t=5:   764ee77c42b4a31e072af13d4352f1da71b084f4
    SHA-1 à t=6:   03f5a89478c2569f39926311df83e04175d6fbdf
    ...
    SHA-1 à t=441: e84d09f6dc7f7fee8760220a6d7ccb92cbe88480
    SHA-1 à t=442: e84d09f6dc7f7fee8760220a6d7ccb92cbe88480
    SHA-1 à t=443: e84d09f6dc7f7fee8760220a6d7ccb92cbe88480
    SHA-1 à t=444: e84d09f6dc7f7fee8760220a6d7ccb92cbe88480
    SHA-1 à t=445: e84d09f6dc7f7fee8760220a6d7ccb92cbe88480
    ```

  - **Code séquentiel :**
    ```
    SHA-1 à t=1:   e351b2af36ea88cdf1f81379a92bfd56483e1b8a
    SHA-1 à t=2:   02455e2cc0e2a18e4029740356af555a14efff66
    SHA-1 à t=3:   3ab075debbdb1a0d745d5acbf957246b0b997ead
    SHA-1 à t=4:   ecb8d32e7f9db66089fafb7fbe700382792a0f85
    SHA-1 à t=5:   0734ef47085e38c79bb0a2e7a59b43f81f8e735e
    ...
    SHA-1 à t=441: 16c0778f0115e0faf296b1cb60bb1c936e07364e
    SHA-1 à t=442: 16c0778f0115e0faf296b1cb60bb1c936e07364e
    SHA-1 à t=443: 16c0778f0115e0faf296b1cb60bb1c936e07364e
    SHA-1 à t=444: 16c0778f0115e0faf296b1cb60bb1c936e07364e
    SHA-1 à t=445: 16c0778f0115e0faf296b1cb60bb1c936e07364e
    ```

- **Conclusion de la validation :**  
  Les valeurs SHA-1 des versions parallèle et séquentielle sont identiques jusqu’à t=2, mais divergent dès t=3 (par exemple, `5947dd2d...` pour la version parallèle contre `3ab075de...` pour la version séquentielle).  
  Cette divergence révèle que la parallélisation perturbe l’évolution de la simulation, probablement à cause d’un ordre de mise à jour variable ou de tirages pseudo-aléatoires non synchronisés entre les threads.  
  Bien que les deux versions atteignent un état stable (hash constant) à partir de t=441, elles ne suivent pas la même trajectoire, et plusieurs états stables sont observés en fin de simulation. Cela indique la nécessité d’une synchronisation plus rigoureuse des accès ou d’un alignement des graines aléatoires pour obtenir une équivalence parfaite avec la version séquentielle.

### Problème rencontré
- **Initialement :**  
  La parallélisation avec OpenMP a été implémentée sans utiliser `#pragma omp critical`.  
  Avec un seul thread, tout fonctionnait correctement.

- **Dès que 2 threads sont utilisés :**  
    L'accès simultané de plusieurs threads aux mêmes éléments de `m_fire_map` ou `next_front` entraînait des écritures concurrentes non synchronisées, ce qui causait une race condition et provoquait immédiatement un segmentation fault.

---

## Partie 2 : Répartition des tâches avec MPI

### Modifications apportées
1. **Séparation de la boucle principale**  
   La boucle est divisée en deux parties distinctes :
   - **Affichage :** Géré par le processus 0.
   - **Calcul :** Géré par les autres processus.

2. **Rôle du processus 0**  
   Le processus 0 s'occupe exclusivement de l'affichage. Il initialise le `Display` et gère la partie visuelle de la boucle.

3. **Échange initial de la géométrie**  
   Dans la première implémentation, le processus 1 envoie, de manière bloquante, la géométrie de la simulation qui est reçue également de manière bloquante par le processus 0.  
   Même si la géométrie est accessible via les arguments de la ligne de commande pour tous les processus, cette approche permet de gérer un cas de géométrie variable dans la simulation (même si ce n'est pas le cas dans ce projet).

4. **Échanges pendant la boucle**  
   La boucle principale comporte un envoi bloquant et une réception non bloquante des cartes de la végétation et du feu.

### Problèmes rencontrés
0. **Séparation inadéquate des boucles**  
   Initialement, une seule boucle combinait affichage et calcul séparé sur les processus, ce qui entraînait l'initialisation de deux fenêtres d'affichage.  
   Cette approche s'est vite avérée inefficace.

1. **Mismatch des types de données**  
   Lors des envois MPI, un décalage entre les types de données a été constaté, problème qui a été rapidement corrigé.

2. **Réception non bloquante inadaptée**  
   Lors de l'initialisation de la géométrie dans le processus 0, l'utilisation d'une réception non bloquante faisait que le processus ne patientait pas pour la géométrie.  
   Cela conduisait à l'initialisation des tailles des cartes avec des valeurs aléatoires, provoquant ainsi des dysfonctionnements.

### Commentaire
Dans cette partie, nous avons testé une approche non bloquante avec `MPI_Irecv` et `MPI_Waitall` afin de permettre un chevauchement entre calcul et communication.  
Cependant, pour ce problème précis, un récepteur bloquant (`MPI_Recv`) serait plus simple et tout aussi efficace, puisque les données sont attendues immédiatement sans travail intermédiaire.  
Le choix des appels non bloquants a été fait en anticipation d'une possible extension dans la Partie 4.

### Résultats
- **Version parallèle (-n 2) :**  
  Temps global moyen par itération : **0.00145327 secondes**

- **Version séquentielle :**  
  Temps global moyen par itération : **0.00121068 secondes**

La surcharge induite par la communication et la synchronisation entre les processus dans la version parallèle dépasse les gains obtenus par la répartition de la charge, rendant l'exécution séquentielle plus rapide.

---

## Partie 3 : Combinaison d'OpenMP et MPI

### Modifications apportées
- Intégration du parallélisme OpenMP dans le code MPI de la Partie 2, en ajoutant une parallélisation de la boucle avec `#pragma omp parallel for`.

### Résultats
- **Exécution sur 2 cœurs et 4 threads :**  
  Temps global moyen par itération : **0.0256515 secondes**

### Bonus : Double Buffering
Pour rendre l’affichage asynchrone par rapport à l’avancement de la simulation, nous avons mis en œuvre la technique du double buffering.  
Cette méthode consiste à alterner entre deux buffers : pendant que l’un est utilisé pour l’affichage, l’autre reçoit les nouvelles données de la simulation.  
Nous prévoyons ainsi une accélération globale grâce à la réduction des temps d’attente, même si celle-ci reste limitée par les surcoûts liés à la gestion des communications non bloquantes.

- **Temps global moyen par itération avec double buffering :**  
  **0.00146456 secondes**
