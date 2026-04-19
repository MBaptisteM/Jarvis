#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// baptiste.mahe@git.forge.epita.fr:p/epita-prepa-computer-science/prog-208-p-05-2029/
// epita-prepa-computer-science-prog-208-p-05-2029-baptiste.mahe.git

// Créer un fichier jarvis (qui sert de repere pour connaitre la base)
// Stocker l'avancée de chaque push des repos
// Stocker le chemin vers ce fichier dans un autre fichier dont je connais toujours l'emplacement

int main(int argc, char* argv[]){
    // Check if there is a git remote
    if (argc < 2)
        errx(EXIT_FAILURE, "ERROR you must enter the git remote to clone your repository");

    
    // Check if it is an epita repo
    int i = 0;
    while (argv[1][i] != NULL && argv[1][i] != '@')
        i++;

    char *reference_str = "@git.forge.epita.fr";

    int j = 0;
    while (argv[1][i + j] != NULL && reference_str[j] != NULL && argv[1][i + j] == reference_str[j])
        j++;
    
    bool is_epita_repo = reference_str[j] == NULL;

    if (!is_epita_repo){
        printf("this string is not an epita repo\n");

        // Clone le repo


        return EXIT_SUCCESS;
    }


    // Extraire le chemin supposé en fonction du nom du clone

    // S1/B1/P1-GaulishVillage-i3-shell-git
    // S1/B1/Project_1-GaulishVillage-i3-shell-git
    // S1/B1/Project_1-i3-shell-git
    // S1/B1/Project_1-GaulishVillage

    char relative_path[512];

    int k = i + j;
    while (argv[1][k] < '0' ||  argv[1][k] > '9')
        k++;

    snprintf(relative_path, sizeof(relative_path), 
            "S%d/B%d/", argv[1][k] - '0', 
            (argv[1][k + 1] - '0') * 10 + (argv[1][k + 2] - '0'));
    k += 3;

    printf("%s\n", &(argv[1][k]));
    printf("%s\n", relative_path);


    // Chercher les dossiers dans l'arborescence
    // Créer les dossiers si ils n'existent pas -> aller dedans sinon
    // Clone

    // Chercher le dossier download
    // Extraire les fichiers si ils existent
    // Si existe : dezip given files
    // Si existe : lire le pdf + créer dossiers/fichiers + remplir .gitignore

    // git add
    // Check gestion du github si activé
    // Clone / Push si github activé (check si clone déjà fait)
    // Envoyer l'utilisateur dans le repo

    return EXIT_SUCCESS;
}