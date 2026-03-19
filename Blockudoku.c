#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "GrilleSDL.h"
#include "Ressources.h"

// Dimensions de la grille de jeu
#define NB_LIGNES   12
#define NB_COLONNES 19

// Nombre de cases maximum par piece
#define NB_CASES    4

// Macros utilisees dans le tableau tab
#define VIDE        0
#define BRIQUE      1
#define DIAMANT     2

int tab[NB_LIGNES][NB_COLONNES]
={ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};

typedef struct
{
  int ligne;
  int colonne;
} CASE;

typedef struct
{
  CASE cases[NB_CASES];
  int  nbCases;
  int  couleur;
} PIECE;

PIECE pieces[12] = { 0,0,0,1,1,0,1,1,4,0,       // carre 4
					 0,0,1,0,2,0,2,1,4,0,       // L 4
					 0,1,1,1,2,0,2,1,4,0,       // J 4
					 0,0,0,1,1,1,1,2,4,0,       // Z 4
					 0,1,0,2,1,0,1,1,4,0,       // S 4
					 0,0,0,1,0,2,1,1,4,0,       // T 4
					 0,0,0,1,0,2,0,3,4,0,       // I 4
					 0,0,0,1,0,2,0,0,3,0,       // I 3
					 0,1,1,0,1,1,0,0,3,0,       // J 3
					 0,0,1,0,1,1,0,0,3,0,       // L 3
					 0,0,0,1,0,0,0,0,2,0,       // I 2
					 0,0,0,0,0,0,0,0,1,0 };     // carre 1

void DessinePiece(PIECE piece);
int  CompareCases(CASE case1,CASE case2);
void TriCases(CASE *vecteur,int indiceDebut,int indiceFin);

///////////////////////////////////////////////////////////////////////////////////////////////////
///// Personal work ///////////////////////////////////////////////////////////////////////////////

#define TRACE(message) printf("[%d::%d] : %s\n", getpid(), *((int*) pthread_getspecific(cleID)), message);
// Global variables
int nbThread = 0;

// Variables message
char* message; // pointeur vers le message à faire défiler
int tailleMessage; // longueur du message
int indiceCourant; // indice du premier caractère à afficher dans la zone graphique
char messageReady = 0;

// Variables piece en cours
PIECE pieceEnCours;

CASE casesInserees[NB_CASES];
int nbCasesInserees;

// Variables score
char MAJScore;
int score;

// Mutex
pthread_mutex_t mutexTID = PTHREAD_MUTEX_INITIALIZER,
                mutexMessage = PTHREAD_MUTEX_INITIALIZER,
                mutexCasesInserees = PTHREAD_MUTEX_INITIALIZER,
                mutexScore = PTHREAD_MUTEX_INITIALIZER;

// Conditions
pthread_cond_t  condMessage,
                condCasesInserees,
                condScore;

// Cles
pthread_key_t cleID;

// Utilities funtions
void setMessage(const char* texte, char signalOn);
PIECE TranslateToOrigin(PIECE piece);
int ComparePieces(PIECE p1, PIECE p2);
void RotationPiece(PIECE* piece);


// Thread functions
void    *threadDefileMessage(void* arg),
        *threadPiece(void* arg),
        *threadEvent(void* arg),
        *threadScore(void* arg);

// Handlers
void HandlerSIGALRM(int sig);

///////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc,char* argv[])
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    pthread_sigmask(SIG_SETMASK, &mask, NULL);

    struct sigaction setALRM;
    setALRM.sa_handler = HandlerSIGALRM;
    setALRM.sa_flags = 0;
    sigemptyset(&(setALRM.sa_mask));
    sigaction(SIGALRM, &setALRM, NULL);

    pthread_key_create(&cleID, NULL);

    pthread_mutex_lock(&mutexTID);
    int ID = nbThread;
    nbThread++;
    pthread_setspecific(cleID, &ID);
    pthread_mutex_unlock(&mutexTID);

    srand((unsigned)time(NULL));

    // Ouverture de la fenetre graphique
    TRACE("Ouverture de la fenetre graphique");

    if (OuvertureFenetreGraphique() < 0)
    {
	    printf("Erreur de OuvrirGrilleSDL\n");
	    exit(1);
    }

    pthread_t   tidDefileMessage = 0,
                tidPiece = 0, 
                tidEvent = 0, 
                tidScore = 0;

    pthread_create(&tidDefileMessage, NULL, threadDefileMessage, NULL);
    setMessage("Lancement du jeu", true);

    pthread_create(&tidPiece, NULL, threadPiece, (void*) NULL);
    pthread_create(&tidEvent, NULL, threadEvent, (void*) NULL);
    pthread_create(&tidScore, NULL, threadScore, NULL);

    TRACE("Tous les threads créés");
    
    pthread_join(tidEvent, NULL);

    // Fermeture de la fenetre
    TRACE("Fermeture de la fenetre graphique...");
    FermetureFenetreGraphique();
    printf("OK\n");

    pthread_key_delete(cleID);

    exit(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/////// Fonctions fournies ////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void DessinePiece(PIECE piece)
{
    int Lmin,Lmax,Cmin,Cmax;
    int largeur,hauteur,Lref,Cref;

    Lmin = piece.cases[0].ligne;
    Lmax = piece.cases[0].ligne;
    Cmin = piece.cases[0].colonne;
    Cmax = piece.cases[0].colonne;

    for (int i=1 ; i<=(piece.nbCases-1) ; i++)
    {
        if (piece.cases[i].ligne > Lmax) Lmax = piece.cases[i].ligne;
        if (piece.cases[i].ligne < Lmin) Lmin = piece.cases[i].ligne;
        if (piece.cases[i].colonne > Cmax) Cmax = piece.cases[i].colonne;
        if (piece.cases[i].colonne < Cmin) Cmin = piece.cases[i].colonne;
    }

    largeur = Cmax - Cmin + 1;
    hauteur = Lmax - Lmin + 1;

    switch(largeur)
    {
        case 1 : Cref = 15; break;
        case 2 : Cref = 15; break;
        case 3 : Cref = 14; break;
        case 4 : Cref = 14; break;  
    }

    switch(hauteur)
    {
        case 1 : Lref = 4; break;
        case 2 : Lref = 4; break;
        case 3 : Lref = 3; break;
        case 4 : Lref = 3; break;
    }

    for (int L=3 ; L<=6 ; L++) for (int C=14 ; C<=17 ; C++) EffaceCarre(L,C);
    for (int i=0 ; i<piece.nbCases ; i++) DessineDiamant(Lref + piece.cases[i].ligne,Cref + piece.cases[i].colonne,piece.couleur);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int CompareCases(CASE case1,CASE case2)
{
    if (case1.ligne < case2.ligne) return -1;
    if (case1.ligne > case2.ligne) return +1;
    if (case1.colonne < case2.colonne) return -1;
    if (case1.colonne > case2.colonne) return +1;
    return 0;
}

void TriCases(CASE *vecteur,int indiceDebut,int indiceFin)
{   // trie les cases de vecteur entre les indices indiceDebut et indiceFin compris
    // selon le critere impose par la fonction CompareCases()
    // Exemple : pour trier un vecteur v de 4 cases, il faut appeler TriCases(v,0,3); 
    int  i,iMin;
    CASE tmp;

    if (indiceDebut >= indiceFin) return;

    // Recherche du minimum
    iMin = indiceDebut;
    for (i=indiceDebut ; i<=indiceFin ; i++)
        if (CompareCases(vecteur[i],vecteur[iMin]) < 0) iMin = i;

    // On place le minimum a l'indiceDebut par permutation
    tmp = vecteur[indiceDebut];
    vecteur[indiceDebut] = vecteur[iMin];
    vecteur[iMin] = tmp;

    // Tri du reste du vecteur par recursivite
    TriCases(vecteur,indiceDebut+1,indiceFin); 
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///// Personal functions //////////////////////////////////////////////////////////////////////////

// Utilities function

void setMessage(const char* texte, char signalOn)
{
    alarm(0);

    pthread_mutex_lock(&mutexMessage);

    // printf("%p", message);
    tailleMessage=strlen(texte);
    message = (char*) realloc(message , tailleMessage+1);
    strcpy(message, texte);
    indiceCourant = 0;

    pthread_cond_signal(&condMessage);

    pthread_mutex_unlock(&mutexMessage);
    
    if(signalOn)
    {
        alarm(0);
        alarm(10);
    }
}

PIECE TranslateToOrigin(PIECE piece)
{
    CASE minCoord = {9,9};

    for(int i = 0; i < piece.nbCases; i++)
    {
        if (piece.cases[i].ligne < minCoord.ligne) minCoord.ligne = piece.cases[i].ligne;
        if (piece.cases[i].colonne < minCoord.colonne) minCoord.colonne = piece.cases[i].colonne;
    }

    for(int i = 0; i < piece.nbCases; i++)
    {
        piece.cases[i].ligne -= minCoord.ligne;
        piece.cases[i].colonne -= minCoord.colonne;
    }

    return piece;
}

int ComparePieces(PIECE p1, PIECE p2)
{
	if(p1.nbCases != p2.nbCases) return 1;

	for(int i = 0; i < p1.nbCases; i++) 
		if(CompareCases(p1.cases[i], p2.cases[i])) return 1;

	return 0;
}

void RotationPiece(PIECE* piece)
{
    for(int i = 0; i < piece->nbCases; i++)
    {
        CASE prevCase = piece->cases[i];

        piece->cases[i].ligne = - prevCase.colonne;
        piece->cases[i].colonne = prevCase.ligne;
    }

    *piece = TranslateToOrigin(*piece);

    TriCases(piece->cases, 0, piece->nbCases-1);
}

// Thread functions
void* threadDefileMessage(void* arg)
{
    pthread_mutex_lock(&mutexTID);
    int ID = nbThread;
    nbThread++;
    pthread_setspecific(cleID, &ID);
    pthread_mutex_unlock(&mutexTID);
    TRACE("threadDefileMessage lancé");

    sigset_t mask;
    sigemptyset(&mask);
    pthread_sigmask(SIG_SETMASK, &mask, NULL);

    TRACE("Mask mit");

    pthread_mutex_lock(&mutexMessage);
    while(message==NULL)
        pthread_cond_wait(&condMessage, &mutexMessage);
    pthread_mutex_unlock(&mutexMessage);

    TRACE("message non nul");

    for(int i = 1; i<=17; i++) DessineLettre(10, i, 0);

    while(true)
    {
        pthread_mutex_lock(&mutexMessage);
        for (int i=0 ; i<tailleMessage; i++) DessineLettre(10,1+i,(i+indiceCourant<tailleMessage? message[i+indiceCourant] : 0));
        indiceCourant=(indiceCourant+1)%(tailleMessage+1);
        pthread_mutex_unlock(&mutexMessage);

        struct timespec spec = {0, 400000000};

        nanosleep(&spec, NULL);
    }

    pthread_exit(NULL);
}

void* threadPiece(void* arg)
{
    pthread_mutex_lock(&mutexTID);
    int ID = nbThread;
    nbThread++;
    pthread_setspecific(cleID, &ID);
    pthread_mutex_unlock(&mutexTID);
    TRACE("threadPiece lancé");

    while(1)
    {
        pieceEnCours = pieces[rand() % 12];
        switch(rand() % 4)
        {
            case 0: pieceEnCours.couleur = JAUNE; break;
            case 1: pieceEnCours.couleur = ROUGE; break;
            case 2: pieceEnCours.couleur = VERT; break;
            case 3: pieceEnCours.couleur = VIOLET; break;
        }

        char nbRotation = rand() % 4;

        for(int i = 0; i < nbRotation; i++)
            RotationPiece(&pieceEnCours);

        DessinePiece(pieceEnCours);

        char replaceWith = VIDE;

        while(replaceWith == VIDE)
        {   
            pthread_mutex_lock(&mutexCasesInserees);
            while(nbCasesInserees<pieceEnCours.nbCases)
                pthread_cond_wait(&condCasesInserees, &mutexCasesInserees);

            TriCases(casesInserees, 0, nbCasesInserees-1);

            replaceWith = (ComparePieces(   TranslateToOrigin((PIECE) {	 casesInserees[0],
                                                                                    casesInserees[1],
                                                                                    casesInserees[2],
                                                                                    casesInserees[3],
                                                                                     nbCasesInserees,
                                                                                     JAUNE }),
                                            pieceEnCours) ? VIDE : BRIQUE );
            
            for(int i = 0; i < 9; i++)
                for(int j = 0; j < 9; j++)
                    if (tab[i][j] == DIAMANT)
                    {
                        tab[i][j] = replaceWith;

                        if (replaceWith == BRIQUE)
                            DessineBrique(i, j, false);

                        else
                            EffaceCarre(i, j);
                    }
            
            nbCasesInserees = 0;

            pthread_mutex_unlock(&mutexCasesInserees);
        }

        pthread_mutex_lock(&mutexScore);
        score+=pieceEnCours.nbCases;
        MAJScore = 1;
        pthread_cond_signal(&condScore);
        pthread_mutex_unlock(&mutexScore);
    }

    return NULL;
}

void* threadEvent(void* arg)
{
    pthread_mutex_lock(&mutexTID);
    int ID = nbThread;
    nbThread++;
    pthread_setspecific(cleID, &ID);
    pthread_mutex_unlock(&mutexTID);
    TRACE("threadEvent lancé");

    EVENT_GRILLE_SDL event;
    char croixClickee = 0;

    while(!croixClickee)
    {
        event = ReadEvent();

        switch(event.type)
        {
            case CROIX:
                croixClickee = 1;
                break;

            case CLIC_GAUCHE:
                if((event.ligne < 9 && event.colonne < 9) && (tab[event.ligne][event.colonne] == VIDE))
                {
                    tab[event.ligne][event.colonne] = DIAMANT;
                    DessineDiamant(event.ligne, event.colonne, pieceEnCours.couleur);

                    pthread_mutex_lock(&mutexCasesInserees);
                    casesInserees[nbCasesInserees] = (CASE) {event.ligne, event.colonne};
                    nbCasesInserees++;
                    pthread_cond_signal(&condCasesInserees);
                    pthread_mutex_unlock(&mutexCasesInserees);
                }

                break;

            case CLIC_DROIT:
                pthread_mutex_lock(&mutexCasesInserees);

                for(int i = 0; i < nbCasesInserees; i++)
                {
                    tab[casesInserees[i].ligne][casesInserees[i].colonne] = VIDE;
                    EffaceCarre(casesInserees[i].ligne,casesInserees[i].colonne);
                }
                
                nbCasesInserees = 0;
                pthread_mutex_unlock(&mutexCasesInserees);
                
                break;
            
            default:
                break;
        }
    }

    pthread_exit(NULL);
}

void* threadScore(void* arg)
{
    pthread_mutex_lock(&mutexTID);
    int ID = nbThread;
    nbThread++;
    pthread_setspecific(cleID, &ID);
    pthread_mutex_unlock(&mutexTID);
    TRACE("threadScore lancé");

    for(int i = 0; i < 4; i++)
        DessineChiffre(1, 17-i, 0);
    while(1)
    {
        pthread_mutex_lock(&mutexScore);
        while (!MAJScore) 
            pthread_cond_wait(&condScore, &mutexScore);

        for(int i = 0; i < 4; i++)
        {
            char printValue = score;
            for(int j = 0; j < i; j++)
                printValue/=10;

            DessineChiffre(1, 17-i, printValue%10);
        }

        MAJScore = 0;

        pthread_mutex_unlock(&mutexScore);
    }
    
}

// Handlers

void HandlerSIGALRM(int sig)
{
    pthread_mutex_unlock(&mutexMessage);

    setMessage("Jeu en cours", false);
}