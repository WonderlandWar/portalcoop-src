#ifndef PCOOP_PROGRESS_H
#define PCOOP_PROGRESS_H

extern void Progress_GetPlayerProgress( int iPlayer, int *piMapProgress, int *piFoundRadios );
extern bool Progress_HasPlayerReachedNumber( int iPlayer, int iMapNumber );
extern bool Progress_HasPlayerFoundRadios( int iPlayer, int iNumRadios );

#endif // PCOOP_PROGRESS_H