/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

void trans_32_32(int M, int N, int A[N][M], int B[M][N]);
void trans_64_64(int M, int N, int A[N][M], int B[M][N]);
void trans_60_68(int M, int N, int A[N][M], int B[M][N]);

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    REQUIRES(M > 0);
    REQUIRES(N > 0);

    if (M == 32 && N == 32)
        trans_32_32(M, N, A, B);
    else if (M == 64 && N == 64)
        trans_64_64(M, N, A, B);
    else if (M == 60 && N == 68)
        trans_60_68(M, N, A, B);
    
    ENSURES(is_transpose(M, N, A, B));
}

void trans_32_32(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k;
    int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;
    for (i = 0; i < N; i += 8) {
        for (j = 0; j < M; j += 8) {
            for (k = 0; k < 8; ++k) {
                tmp1 = A[i+k][j];
                tmp2 = A[i+k][j+1];
                tmp3 = A[i+k][j+2];
                tmp4 = A[i+k][j+3];
                tmp5 = A[i+k][j+4];
                tmp6 = A[i+k][j+5];
                tmp7 = A[i+k][j+6];
                tmp8 = A[i+k][j+7];

                B[j][i+k] = tmp1;
                B[j+1][i+k] = tmp2;
                B[j+2][i+k] = tmp3;
                B[j+3][i+k] = tmp4;
                B[j+4][i+k] = tmp5;
                B[j+5][i+k] = tmp6;
                B[j+6][i+k] = tmp7;
                B[j+7][i+k] = tmp8;
            }
        }
    }
}

void trans_64_64(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k;
    int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;
    for (i = 0; i < N; i += 8) {
        for (j = 0; j < M; j += 8) {
            for (k = 0; k < 4; ++k) {
                tmp1 = A[i+k][j];
                tmp2 = A[i+k][j+1];
                tmp3 = A[i+k][j+2];
                tmp4 = A[i+k][j+3];
                tmp5 = A[i+k][j+4];
                tmp6 = A[i+k][j+5];
                tmp7 = A[i+k][j+6];
                tmp8 = A[i+k][j+7];

                B[j][i+k] = tmp1;
                B[j+1][i+k] = tmp2;
                B[j+2][i+k] = tmp3;
                B[j+3][i+k] = tmp4;

                B[j][i+k+4] = tmp5;
                B[j+1][i+k+4] = tmp6;
                B[j+2][i+k+4] = tmp7;
                B[j+3][i+k+4] = tmp8;
            }
            for (k = 0; k < 4; ++k) {
                tmp1 = A[i+4][j+k];
                tmp2 = A[i+5][j+k];
                tmp3 = A[i+6][j+k];
                tmp4 = A[i+7][j+k];

                tmp5 = B[j+k][i+4];
                tmp6 = B[j+k][i+5];
                tmp7 = B[j+k][i+6];
                tmp8 = B[j+k][i+7];

                B[j+k][i+4] = tmp1;
                B[j+k][i+5] = tmp2;
                B[j+k][i+6] = tmp3;
                B[j+k][i+7] = tmp4;

                B[j+k+4][i] = tmp5;
                B[j+k+4][i+1] = tmp6;
                B[j+k+4][i+2] = tmp7;
                B[j+k+4][i+3] = tmp8;
            }
            for (k = 0; k < 4; ++k) {
                tmp1 = A[i+k+4][j+4];
                tmp2 = A[i+k+4][j+5];
                tmp3 = A[i+k+4][j+6];
                tmp4 = A[i+k+4][j+7];

                B[j+4][i+k+4] = tmp1;
                B[j+5][i+k+4] = tmp2;
                B[j+6][i+k+4] = tmp3;
                B[j+7][i+k+4] = tmp4;
            }
        }
    }
}

void trans_60_68(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k, l;
    int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;
    for (i = 0; i < 64; i += 8) {
        for (j = 0; j < 56; j += 8) {
            for (k = 0; k < 8; ++k) {
                tmp1 = A[i+k][j];
                tmp2 = A[i+k][j+1];
                tmp3 = A[i+k][j+2];
                tmp4 = A[i+k][j+3];
                tmp5 = A[i+k][j+4];
                tmp6 = A[i+k][j+5];
                tmp7 = A[i+k][j+6];
                tmp8 = A[i+k][j+7];

                B[j+k][i] = tmp1;
                B[j+k][i+1] = tmp2;
                B[j+k][i+2] = tmp3;
                B[j+k][i+3] = tmp4;
                B[j+k][i+4] = tmp5;
                B[j+k][i+5] = tmp6;
                B[j+k][i+6] = tmp7;
                B[j+k][i+7] = tmp8;
            }
            for (k = 0; k < 8; ++k) {
                for (l = 0; l < k; ++l) {
                    tmp1 = B[j+k][i+l];
                    B[j+k][i+l] = B[j+l][i+k];
                    B[j+l][i+k] = tmp1;
                }
            }
        }
    }
    for (j = 0; j < 56; j += 4) {
        for (k = 0; k < 4; ++k) {
            tmp1 = A[64+k][j];
            tmp2 = A[64+k][j+1];
            tmp3 = A[64+k][j+2];
            tmp4 = A[64+k][j+3];

            B[j+k][64] = tmp1;
            B[j+k][65] = tmp2;
            B[j+k][66] = tmp3;
            B[j+k][67] = tmp4;
        }
        for (k = 0; k < 4; ++k) {
            for (l = 0; l < k; ++l) {
                tmp1 = B[j+k][64+l];
                B[j+k][64+l] = B[j+l][64+k];
                B[j+l][64+k] = tmp1;
            }
        }
    }
    for (i = 0; i < 68; i += 4) {
        for (k = 0; k < 4; ++k) {
            tmp1 = A[i+k][56];
            tmp2 = A[i+k][57];
            tmp3 = A[i+k][58];
            tmp4 = A[i+k][59];

            B[56+k][i] = tmp1;
            B[56+k][i+1] = tmp2;
            B[56+k][i+2] = tmp3;
            B[56+k][i+3] = tmp4;
        }
        for (k = 0; k < 4; ++k) {
            for (l = 0; l < k; ++l) {
                tmp1 = B[56+k][i+l];
                B[56+k][i+l] = B[56+l][i+k];
                B[56+l][i+k] = tmp1;
            }
        }
    }
}


/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    // registerTransFunction(trans_64_64, trans_64_64_desc);

}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

