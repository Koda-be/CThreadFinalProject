#include <SDL/SDL_stdinc.h>
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

char* printFormat = NULL;
pthread_mutex_t mutexStdout = PTHREAD_MUTEX_INITIALIZER;

#define TRACE(format,...)   pthread_mutex_lock(&mutexStdout);                                                       \
                            printFormat = malloc(strlen(format)+1);                                                 \
                            strcpy(printFormat, format);                                                            \
                            sprintf(printFormat, printFormat __VA_OPT__(,) __VA_ARGS__);                            \
                            printf("[%d::%d] : %s\n", getpid(), *((int*) pthread_getspecific(cleID)), printFormat); \
                            free(printFormat);                                                                      \
                            pthread_mutex_unlock(&mutexStdout)                                                      \


int nbThread = 0;

char* message;
int tailleMessage = 0;
int indiceCourant = 0;
char messageReady = 0;


PIECE pieceEnCours;

CASE casesInserees[NB_CASES];
int nbCasesInserees = 0;


char MAJScore = 0;
char MAJCombo = 0;
int score = 0;
int combos = 0;


int lignesCompletes[NB_CASES] = {0, 0, 0, 0},
    nbLignesCompletes = 0,
    colonnesCompletes[NB_CASES] = {0, 0, 0, 0},
    nbColonnesCompletes = 0,
    carresComplets[NB_CASES] = {0, 0, 0, 0},
    nbCarresComplets = 0,
    nbAnalyses = 0;

int couleurActuelle;

char traitementEnCours = 0;


pthread_t   tidDefileMessage,
            tidPiece, 
            tidEvent,
            tidScore,
            tidCase[9][9],
            tidNettoyeur;


pthread_mutex_t mutexTID = PTHREAD_MUTEX_INITIALIZER,
                mutexMessage = PTHREAD_MUTEX_INITIALIZER,
                mutexCasesInserees = PTHREAD_MUTEX_INITIALIZER,
                mutexScore = PTHREAD_MUTEX_INITIALIZER,
                mutexAnalyse = PTHREAD_MUTEX_INITIALIZER,
                mutexTraitement = PTHREAD_MUTEX_INITIALIZER,
                mutexLed = PTHREAD_MUTEX_INITIALIZER;


pthread_cond_t  condMessage = PTHREAD_COND_INITIALIZER,
                condCasesInserees = PTHREAD_COND_INITIALIZER,
                condScore = PTHREAD_COND_INITIALIZER,
                condAnalyse = PTHREAD_COND_INITIALIZER,
                condTraitement = PTHREAD_COND_INITIALIZER;


pthread_key_t   cleID,
                cleCase;


pthread_once_t  onceID = PTHREAD_ONCE_INIT,
                onceCase = PTHREAD_ONCE_INIT;


void setThreadID(void);
void setMessage(const char* texte, char signalOn);
PIECE TranslateToOrigin(PIECE piece);
int ComparePieces(PIECE p1, PIECE p2);
void RotationPiece(PIECE* piece);


void    InitCleID(void),
        InitCleCase(void);


void    *threadDefileMessage(void* arg),
        *threadPiece(void* arg),
        *threadEvent(void* arg),
        *threadScore(void* arg),
        *threadCase(void* arg),
        *threadNettoyeur(void* arg);


void    HandlerSIGALRM(int sig),
        HandlerSIGUSR1(int sig);

///////////////////////////////////////////////////////////////////////////////////////////////////


int main(int argc,char* argv[])
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGUSR1);
    pthread_sigmask(SIG_SETMASK, &mask, NULL);


    struct sigaction setALRM;
    setALRM.sa_handler = HandlerSIGALRM;
    setALRM.sa_flags = 0;
    sigemptyset(&(setALRM.sa_mask));
    sigaction(SIGALRM, &setALRM, NULL);

    struct sigaction setUSR1;
    setUSR1.sa_handler = HandlerSIGUSR1;
    setUSR1.sa_flags = 0;
    sigemptyset(&(setUSR1.sa_mask));
    sigaction(SIGUSR1, &setUSR1, NULL);

    setThreadID();

    srand((unsigned)time(NULL));

    // Ouverture de la fenetre graphique
    TRACE("Ouverture de la fenetre graphique");

    if (OuvertureFenetreGraphique() < 0)
    {
	    printf("Erreur de OuvrirGrilleSDL\n");
	    exit(1);
    }

    DessineVoyant(8, 10, VERT);
    couleurActuelle = VERT;

    pthread_create(&tidDefileMessage, NULL, threadDefileMessage, NULL);
    setMessage("Lancement du jeu", true);

    pthread_create(&tidPiece, NULL, threadPiece, NULL);
    pthread_create(&tidEvent, NULL, threadEvent, NULL);
    pthread_create(&tidScore, NULL, threadScore, NULL);

    for(int i = 0; i < 9; i++)
        for(int j = 0; j < 9; j++)
        {
            CASE* casePtr = malloc(sizeof(CASE));
            *casePtr = (CASE) { i, j};
            pthread_create(&tidCase[i][j], NULL, threadCase, casePtr);
        }

    pthread_create(&tidNettoyeur, NULL, threadNettoyeur, NULL);
    TRACE("Tous les threads créés, attente de join le threadPiece");

    pthread_join(tidPiece, NULL);
#ifdef DEBUG
    TRACE("ThreadPiece joined");
#endif

    setMessage("Fin de partie", false);
#ifdef DEBUG
    TRACE("Message sent");
#endif

    DessineVoyant(8,10,ROUGE);

    pthread_cancel(tidEvent);

#ifdef DEBUG
        TRACE("threadEvent cancelled");
        nanosleep(&((struct timespec){0, 500000000}), NULL);
        TRACE("deleting threadCases");
#endif

    for(int i = 0; i < 9; i++)
        for(int j = 0; j < 9; j++)
        {
#ifdef DEBUG
            TRACE("pthread_cancel de la case %d %d", i, j);
#endif
            pthread_cancel(tidCase[i][j]);
        }

    char croixClickee = 0;
    
    while(!croixClickee)
    {
        EVENT_GRILLE_SDL event = ReadEvent();

        if(event.type == CROIX) croixClickee = 1;
    }

    pthread_cancel(tidDefileMessage);

#ifdef DEBUG
    TRACE("threadDefileMessage cancelled");
#endif
    // Fermeture de la fenetre
    TRACE("Fermeture de la fenetre graphique...");
    FermetureFenetreGraphique();
    TRACE("OK");

    pthread_key_delete(cleID);
    pthread_key_delete(cleCase);

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

void setThreadID(void)
{
    pthread_once(&onceID, InitCleID);

    pthread_mutex_lock(&mutexTID);
    int* ID = malloc(sizeof(int));
    *ID = nbThread;
    nbThread++;
    pthread_setspecific(cleID, ID);
    pthread_mutex_unlock(&mutexTID);
}

void setMessage(const char* texte, char signalOn)
{
    alarm(0);
#ifdef DEBUG
    TRACE("In setMessage");
#endif

    pthread_mutex_lock(&mutexMessage);

    tailleMessage=strlen(texte);
    message = (char*) realloc(message , tailleMessage+1);
    strcpy(message, texte);
    indiceCourant = 0;

    pthread_cond_signal(&condMessage);

    pthread_mutex_unlock(&mutexMessage);
    
    if(signalOn)
        alarm(5);

#ifdef DEBUG
    TRACE("exiting setMessage");
#endif
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



void InitCleID(void)
{
    pthread_key_create(&cleID,free);
}

#ifdef DEBUG
void DestroyKey(void* arg)
{
    printf("Entering DestroyKey\n");                                            
    CASE* casePtr = (CASE*) arg;                                                
    printf("Destroying specific of %d %d\n", casePtr->ligne, casePtr->colonne); 
    free(casePtr);                                                              
}                                                                               
#endif

void InitCleCase(void)
{
#ifdef DEBUG
#define destroyFunc DestroyKey
#else
#define destroyFunc free
#endif
    pthread_key_create(&cleCase, destroyFunc);
}



#ifdef DEBUG
void cleanupRoutine(void* ptr)
{
    TRACE("entered cleanup routine");
    free(ptr);
    TRACE("sortie de cleanup routine");
}
#define cleanuproutine cleanupRoutine
#else
#define cleanuproutine free
#endif

void* threadDefileMessage(void* arg)
{
    setThreadID();
    TRACE("threadDefileMessage lancé");

    pthread_cleanup_push(cleanuproutine, message);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    pthread_sigmask(SIG_SETMASK, &mask, NULL);

    pthread_mutex_lock(&mutexMessage);
    while(message==NULL)
        pthread_cond_wait(&condMessage, &mutexMessage);
    pthread_mutex_unlock(&mutexMessage);

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

    pthread_cleanup_pop(1);

    pthread_exit(NULL);
}

void* threadPiece(void* arg)
{
    setThreadID();
    TRACE("threadPiece lancé");

    char piecePlaceable = 1;

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

        pthread_mutex_lock(&mutexTraitement);

        piecePlaceable = 0;

        for(int i = 0; i < 9 && !piecePlaceable; i++)
        {
            for(int j = 0; j < 9 && !piecePlaceable; j++)
            {
                char nbEmplacement = 0;
                for(int k = 0; k < pieceEnCours.nbCases; k++)
                {
                    CASE caseCheckee = {i+pieceEnCours.cases[k].ligne, j+pieceEnCours.cases[k].colonne};
                    if(caseCheckee.ligne < 9 && caseCheckee.colonne < 9 && tab[caseCheckee.ligne][caseCheckee.colonne] == VIDE) nbEmplacement++;
                    else break;
                }
                if(nbEmplacement == pieceEnCours.nbCases)
                    piecePlaceable = 1;
            }
        }

        if(!piecePlaceable) break;

        pthread_mutex_unlock(&mutexTraitement);

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
            
            if(replaceWith == VIDE) nbCasesInserees = 0;

            pthread_mutex_unlock(&mutexCasesInserees);
        }

        pthread_mutex_lock(&mutexTraitement);
        traitementEnCours = true;
        pthread_mutex_unlock(&mutexTraitement);


        pthread_mutex_lock(&mutexLed);
        couleurActuelle = BLEU;
        DessineVoyant(8, 10,couleurActuelle);
        pthread_mutex_unlock(&mutexLed);

        pthread_mutex_lock(&mutexScore);
        score+=pieceEnCours.nbCases;
        MAJScore = 1;
        pthread_cond_signal(&condScore);
        pthread_mutex_unlock(&mutexScore);

        for(int i = 0; i < nbCasesInserees; i++)
            pthread_kill(tidCase[casesInserees[i].ligne][casesInserees[i].colonne], SIGUSR1);

        nbCasesInserees = 0;

        pthread_mutex_lock(&mutexTraitement);
        while(traitementEnCours)
            pthread_cond_wait(&condTraitement, &mutexTraitement);
        pthread_mutex_unlock(&mutexTraitement);
    }

#ifdef DEBUG
    TRACE("Sortie du threadPiece");
#endif
    pthread_exit(NULL);
}

#ifdef DEBUG
void cleanupEvent(void* arg)
{
    TRACE("cleanupEvent");
}
#endif

void* threadEvent(void* arg)
{
    setThreadID();
    TRACE("threadEvent lancé");

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

#ifdef DEBUG
    pthread_cleanup_push(cleanupEvent, NULL);
#endif

    EVENT_GRILLE_SDL event;

    while(1)
    {
        event = ReadEvent();

        switch(event.type)
        {
            case CROIX:
                exit(0);

            case CLIC_GAUCHE:
                if((event.ligne < 9 && event.colonne < 9) && (tab[event.ligne][event.colonne] == VIDE) && !traitementEnCours)
                {
                    tab[event.ligne][event.colonne] = DIAMANT;
                    DessineDiamant(event.ligne, event.colonne, pieceEnCours.couleur);

                    pthread_mutex_lock(&mutexCasesInserees);
                    casesInserees[nbCasesInserees] = (CASE) {event.ligne, event.colonne};
                    nbCasesInserees++;
                    pthread_cond_signal(&condCasesInserees);
                    pthread_mutex_unlock(&mutexCasesInserees);
                }

                else 
                {
                    pthread_mutex_lock(&mutexLed);
                    DessineVoyant(8,10,ROUGE);
                    nanosleep(&((struct timespec) {0, 400000000}), NULL);
                    DessineVoyant(8,10,couleurActuelle);
                    pthread_mutex_unlock(&mutexLed);
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

#ifdef DEBUG
    pthread_cleanup_pop(1);
#endif
    pthread_exit(NULL);
}

void* threadScore(void* arg)
{
    setThreadID();
    TRACE("threadScore lancé");

    for(int i = 0; i < 4; i++)
        DessineChiffre(1, 17-i, 0);
    while(1)
    {
        pthread_mutex_lock(&mutexScore);
        while (!MAJScore && !MAJCombo) 
            pthread_cond_wait(&condScore, &mutexScore);

        for(int i = 0; i < 4; i++)
        {
            int     printScore = score,
                    printCombo = combos;
            for(int j = 0; j < i; j++)
            {
                printScore/=10;
                printCombo/=10;
            }

            DessineChiffre(1, 17-i, printScore%10);
            DessineChiffre(8, 17-i, printCombo%10);
        }

        MAJScore = 0;
        MAJCombo = 0;

        pthread_mutex_unlock(&mutexScore);
    }
    
}

void* threadCase(void* arg)
{
    setThreadID();

    pthread_once(&onceCase, InitCleCase);

    TRACE("threadCase lancé");

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    pthread_sigmask(SIG_SETMASK, &mask, NULL);

    pthread_setspecific(cleCase, (CASE*) arg);

    while(1)
    {
        pause();
    }
}

void* threadNettoyeur(void* arg)
{
    setThreadID();
    TRACE("threadNettoyeur lancé");

    while(1)
    {
        pthread_mutex_lock(&mutexAnalyse);

        while(nbAnalyses < pieceEnCours.nbCases)
            pthread_cond_wait(&condAnalyse, &mutexAnalyse);
            
        pthread_mutex_unlock(&mutexAnalyse);

        int addCombo = nbLignesCompletes + nbColonnesCompletes + nbCarresComplets;

        pthread_mutex_lock(&mutexScore);

        if((MAJCombo = addCombo))
        {
            nanosleep(&((struct timespec) {2, 0 }), NULL);

            for(int i = 0; i < nbLignesCompletes; i++)
                for(int j = 0; j < 9; j++)
                {
                    tab[lignesCompletes[i]][j] = VIDE;
                    EffaceCarre(lignesCompletes[i], j);
                }

            nbLignesCompletes = 0;

            for(int i = 0; i < 9; i++)
                for(int j = 0; j < nbColonnesCompletes; j++)
                {
                    tab[i][colonnesCompletes[j]] = VIDE;
                    EffaceCarre(i, colonnesCompletes[j]);
                }

            nbColonnesCompletes = 0;

            for(int i = 0; i < nbCarresComplets; i++)
            {
                CASE carre = {(carresComplets[i]%3)*3, (carresComplets[i]/3)*3};

                for(int i = carre.ligne; i < carre.ligne+3; i++)
                    for(int j = carre.colonne; j < carre.colonne+3; j++)
                    {
                        tab[i][j] = VIDE;
                        EffaceCarre(i,j);
                    }
            }

            nbCarresComplets = 0;
        }

        nbAnalyses = 0;

        combos += addCombo;

        switch(addCombo)
        {
            case 0: break;
            case 1: score += 10; setMessage("Simple combo", false); break;
            case 2: score += 25; setMessage("Double combo", false); break;
            case 3: score += 40; setMessage("Triple combo", false); break;
            default: score += 55; setMessage("Quadruple combo", false); break;
        }

        pthread_mutex_unlock(&mutexScore);
        pthread_cond_signal(&condScore);

        pthread_mutex_lock(&mutexLed);
        couleurActuelle = VERT;
        DessineVoyant(8,10,VERT);
        pthread_mutex_unlock(&mutexLed);

        pthread_mutex_lock(&mutexTraitement);
        traitementEnCours = false;
        pthread_cond_signal(&condTraitement);
        pthread_mutex_unlock(&mutexTraitement);
    }
}



void HandlerSIGALRM(int sig)
{
    pthread_mutex_unlock(&mutexMessage);

    setMessage("Jeu en cours", false);
}

void HandlerSIGUSR1(int sig)
{   
    CASE* casePtr = pthread_getspecific(cleCase);

    CASE carre;
    int carreInt = 0;

    char    countLigne = 0,
            countColonne = 0,
            countCarre = 0;

    char    checkLigne = 1, 
            checkColonne = 1, 
            checkCarre = 1;

    carre.ligne = (casePtr->ligne/3)*3;
    carre.colonne = (casePtr->colonne/3)*3;
    carreInt = casePtr->ligne/3 + (casePtr->colonne - (casePtr->colonne % 3));


    pthread_mutex_lock(&mutexAnalyse);
    
    for(int i = 0; i < nbLignesCompletes; i++)
        if(lignesCompletes[i] == casePtr->ligne)
        {
            checkLigne = 0;
            break;
        }

    for(int i = 0; i < nbColonnesCompletes; i++)
        if(colonnesCompletes[i] == casePtr->colonne)
        {
            checkColonne = 0;
            break;
        }

    for(int i = 0; i < nbCarresComplets; i++)
        if(carresComplets[i] == carreInt)
        {
            checkCarre = 0;
            break;
        }

    if(checkLigne)
    {
        
        for(countLigne = 0; countLigne < 9 && tab[casePtr->ligne][countLigne] == BRIQUE; countLigne++);

        if(countLigne == 9)
        {
            lignesCompletes[nbLignesCompletes] = casePtr->ligne;
            nbLignesCompletes++;

            for(int i = 0; i < 9; i++)
                DessineBrique(casePtr->ligne, i, true);
        }
    }    

    if(checkColonne)
    {
        for(countColonne = 0; countColonne < 9 && tab[countColonne][casePtr->colonne] == BRIQUE; countColonne++);

        if(countColonne == 9)
        {
            colonnesCompletes[nbColonnesCompletes] = casePtr->colonne;
            nbColonnesCompletes++;

            for(int i = 0; i < 9; i++)
                DessineBrique(i, casePtr->colonne, true);
        }
    }

    if(checkCarre)
    {
        for(int i = carre.ligne; i < carre.ligne + 3; i++)
            for(int j = carre.colonne; j < carre.colonne + 3; j++)
                if(tab[i][j] == BRIQUE) countCarre++;
                
        
        if(countCarre == 9)
        {
            carresComplets[nbCarresComplets] = carreInt;
            nbCarresComplets++;

            for(int i = carre.ligne; i < carre.ligne + 3; i++)
                for(int j = carre.colonne; j < carre.colonne + 3; j++)
                    DessineBrique(i, j, true);
        }
    }

    nbAnalyses++;
    pthread_cond_signal(&condAnalyse);

    pthread_mutex_unlock(&mutexAnalyse);
}