/*
 * fgcolor.c
 *
 * Color ROM for ABC800C/M fine graphics
 */
#include "compiler.h"
#include "screen.h"

#define K 0			/* Black */
#define R 1			/* Red */
#define G 2			/* Green */
#define Y 3			/* Yellow */
#define B 4			/* Blue */
#define M 5			/* Magenta */
#define C 6			/* Cyan */
#define W 7			/* White */

#define P(a,b) { a, b, a, b }, { a, a, b, b } /* Color pair */

const uint8_t fgcolor[128][4] = {
    { K, K, K, K }, { K, W, W, W }, { K, R, G, Y }, { K, R, G, B },
    { K, R, G, M }, { K, R, G, C }, { K, R, G, W }, { K, R, Y, B },
    { K, R, Y, M }, { K, R, Y, C }, { K, R, Y, W }, { K, R, B, M },
    { K, R, B, C }, { K, R, B, W }, { K, R, M, C }, { K, R, M, W },

    { K, R, C, W }, { K, G, Y, B }, { K, G, Y, M }, { K, G, Y, C },
    { K, G, Y, W }, { K, G, B, M }, { K, G, B, C }, { K, G, B, W },
    { K, G, M, C }, { K, G, M, W }, { K, G, K, W }, { K, Y, B, M },
    { K, Y, B, C }, { K, Y, B, W }, { K, Y, M, C }, { K, Y, M, W },

    { K, Y, C, W }, { K, B, M, C }, { K, B, M, W }, { K, B, C, W },
    { K, M, C, W }, { R, G, Y, B }, { R, G, Y, M }, { R, G, Y, C },
    { R, G, Y, W }, { R, G, B, M }, { R, G, B, C }, { R, G, B, W },
    { R, G, M, C }, { R, G, M, W }, { R, G, C, W }, { R, Y, B, M },

    { R, Y, B, C }, { R, Y, B, W }, { R, Y, M, C }, { R, Y, M, W },
    { R, Y, C, W }, { R, B, M, C }, { R, B, M, W }, { R, B, C, W },
    { R, M, C, W }, { G, Y, B, M }, { G, Y, B, C }, { G, Y, B, W },
    { G, Y, M, C }, { G, Y, M, W }, { G, Y, C, W }, { G, B, M, C },

    { G, B, M, W }, { G, B, C, W }, { G, M, C, W }, { Y, B, M, C },
    { Y, B, M, W }, { Y, B, C, W }, { Y, M, C, W }, { B, M, C, W },
    P(K,R), P(K,G), P(K,Y), P(K,B),
    P(K,M), P(K,C), P(K,W), P(R,G), P(R,Y), P(R,B), P(R,M), P(R,C),

    P(R,W), P(G,Y), P(G,B), P(G,M), P(G,C), P(G,W), P(Y,B), P(Y,M),
    P(Y,C), P(Y,W), P(B,M), P(B,C), P(B,W), P(M,C), P(M,W), P(C,W)
};
