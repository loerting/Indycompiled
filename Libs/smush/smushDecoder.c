#include "smushDecoder.h"

#include <stdlib.h>
#include <string.h>

#define SMUSH_TAG(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

#define SMUSHTAG_SANM SMUSH_TAG('S', 'A', 'N', 'M')
#define SMUSHTAG_SHDR SMUSH_TAG('S', 'H', 'D', 'R')
#define SMUSHTAG_FLHD SMUSH_TAG('F', 'L', 'H', 'D')
#define SMUSHTAG_FRME SMUSH_TAG('F', 'R', 'M', 'E')
#define SMUSHTAG_BL16 SMUSH_TAG('B', 'l', '1', '6')
#define SMUSHTAG_WAVE SMUSH_TAG('W', 'a', 'v', 'e')

// Bl16 frame header (little-endian), followed by the codec data
#define BL16_WIDTH        0x08
#define BL16_HEIGHT       0x0C
#define BL16_SEQNUM       0x10
#define BL16_CODEC        0x12
#define BL16_ROTATION     0x13  // how the reference buffers rotate after this frame
#define BL16_FILLCOLORS   0x18  // 4 colors for opcodes 0xF9-0xFC
#define BL16_CLEARCOLOR   0x20  // color of the reference buffers at a key frame
#define BL16_RLESIZE      0x24  // decoded size of a codec 5 frame
#define BL16_CODEBOOK     0x28  // 256 colors
#define BL16_HEADERSIZE   0x230

#define BL16_MAXCOPYOFFSET 32768 // motion offsets are 16-bit pixel counts

#ifdef SMUSH_TEST_FFMPEG
// Test builds only (Scripts/indy/test_smush.sh): motion copies whose source block lies partly outside the
// picture. The original reads whatever memory follows its buffer there; FFmpeg skips these copies.
int SmushDecoder_numOutsideCopies;
#endif

typedef enum eSmushBufferIndex
{
    SMUSHBUF_PREV2 = 0, // second reference picture (opcode 0xF6, codec 4)
    SMUSHBUF_PREV1 = 1, // first reference picture: source of motion vectors (codec 3)
    SMUSHBUF_CUR   = 2, // picture being decoded
} SmushBufferIndex;

typedef struct sSmushGlyph4
{
    uint8_t numFg;
    uint8_t numBg;
    uint16_t aFgOffsets[16];
    uint16_t aBgOffsets[16];
} SmushGlyph4;

typedef struct sSmushGlyph8
{
    uint8_t numFg;
    uint8_t numBg;
    uint16_t aFgOffsets[64];
    uint16_t aBgOffsets[64];
} SmushGlyph8;

typedef struct sSmushGlyphPixels
{
    uint8_t numFg;
    uint8_t numBg;
    uint8_t aFg[64]; // pixel index (row * size + col) of pixels on the edge or the filled side
    uint8_t aBg[64];
} SmushGlyphPixels;

struct sSmushDecoder
{
    SmushReadFunc pfRead;
    void* pUser;
    SmushInfo info;

    uint8_t* pChunk;
    size_t chunkCapacity;

    // Video: three pictures that swap roles; each one has a guard band so any 16-bit motion offset stays in memory
    uint16_t* apBufferMem[3];
    uint16_t* apBuffer[3];
    size_t bufferGuard;
    int bufWidth;
    int bufHeight;
    uint16_t* pLastPicture;
    int lastSeqNum;
    int offsetsPitch;
    int16_t aMotionOffsets[245];
    SmushGlyphPixels aGlyph4Pixels[256];
    SmushGlyphPixels aGlyph8Pixels[256];
    SmushGlyph4 aGlyph4[256];
    SmushGlyph8 aGlyph8[256];

    // Audio
    int16_t* pPcm;
    size_t pcmCapacity;
    uint16_t aVimaDelta[89][64];

    int frameNum;
    int bPendingFrame;
    size_t pendingSize;
};

typedef struct sSmushBitReader
{
    const uint8_t* pCur;
    const uint8_t* pEnd;
} SmushBitReader;

typedef struct sSmushBlockContext
{
    const uint8_t* pCur;
    const uint8_t* pEnd;
    int bOverrun;
    ptrdiff_t pitch;
    ptrdiff_t numPixels;
    uint16_t* pPicture;
    const uint16_t* pPrev1;
    const uint16_t* pPrev2;
    const int16_t* aMotionOffsets;
    const SmushGlyph4* aGlyph4;
    const SmushGlyph8* aGlyph8;
    uint16_t aFillColors[4];
    uint16_t aCodebook[256];
} SmushBlockContext;

static const int8_t SmushDecoder_aMotionVectors[245][2] = {
    {   0,   0 }, {  -1, -43 }, {   6, -43 }, {  -9, -42 }, {  13, -41 }, { -16, -40 }, {  19, -39 }, { -23, -36 },
    {  26, -34 }, {  -2, -33 }, {   4, -33 }, { -29, -32 }, {  -9, -32 }, {  11, -31 }, { -16, -29 }, {  32, -29 },
    {  18, -28 }, { -34, -26 }, { -22, -25 }, {  -1, -25 }, {   3, -25 }, {  -7, -24 }, {   8, -24 }, {  24, -23 },
    {  36, -23 }, { -12, -22 }, {  13, -21 }, { -38, -20 }, {   0, -20 }, { -27, -19 }, {  -4, -19 }, {   4, -19 },
    { -17, -18 }, {  -8, -17 }, {   8, -17 }, {  18, -17 }, {  28, -17 }, {  39, -17 }, { -12, -15 }, {  12, -15 },
    { -21, -14 }, {  -1, -14 }, {   1, -14 }, { -41, -13 }, {  -5, -13 }, {   5, -13 }, {  21, -13 }, { -31, -12 },
    { -15, -11 }, {  -8, -11 }, {   8, -11 }, {  15, -11 }, {  -2, -10 }, {   1, -10 }, {  31, -10 }, { -23,  -9 },
    { -11,  -9 }, {  -5,  -9 }, {   4,  -9 }, {  11,  -9 }, {  42,  -9 }, {   6,  -8 }, {  24,  -8 }, { -18,  -7 },
    {  -7,  -7 }, {  -3,  -7 }, {  -1,  -7 }, {   2,  -7 }, {  18,  -7 }, { -43,  -6 }, { -13,  -6 }, {  -4,  -6 },
    {   4,  -6 }, {   8,  -6 }, { -33,  -5 }, {  -9,  -5 }, {  -2,  -5 }, {   0,  -5 }, {   2,  -5 }, {   5,  -5 },
    {  13,  -5 }, { -25,  -4 }, {  -6,  -4 }, {  -3,  -4 }, {   3,  -4 }, {   9,  -4 }, { -19,  -3 }, {  -7,  -3 },
    {  -4,  -3 }, {  -2,  -3 }, {  -1,  -3 }, {   0,  -3 }, {   1,  -3 }, {   2,  -3 }, {   4,  -3 }, {   6,  -3 },
    {  33,  -3 }, { -14,  -2 }, { -10,  -2 }, {  -5,  -2 }, {  -3,  -2 }, {  -2,  -2 }, {  -1,  -2 }, {   0,  -2 },
    {   1,  -2 }, {   2,  -2 }, {   3,  -2 }, {   5,  -2 }, {   7,  -2 }, {  14,  -2 }, {  19,  -2 }, {  25,  -2 },
    {  43,  -2 }, {  -7,  -1 }, {  -3,  -1 }, {  -2,  -1 }, {  -1,  -1 }, {   0,  -1 }, {   1,  -1 }, {   2,  -1 },
    {   3,  -1 }, {  10,  -1 }, {  -5,   0 }, {  -3,   0 }, {  -2,   0 }, {  -1,   0 }, {   1,   0 }, {   2,   0 },
    {   3,   0 }, {   5,   0 }, {   7,   0 }, { -10,   1 }, {  -7,   1 }, {  -3,   1 }, {  -2,   1 }, {  -1,   1 },
    {   0,   1 }, {   1,   1 }, {   2,   1 }, {   3,   1 }, { -43,   2 }, { -25,   2 }, { -19,   2 }, { -14,   2 },
    {  -5,   2 }, {  -3,   2 }, {  -2,   2 }, {  -1,   2 }, {   0,   2 }, {   1,   2 }, {   2,   2 }, {   3,   2 },
    {   5,   2 }, {   7,   2 }, {  10,   2 }, {  14,   2 }, { -33,   3 }, {  -6,   3 }, {  -4,   3 }, {  -2,   3 },
    {  -1,   3 }, {   0,   3 }, {   1,   3 }, {   2,   3 }, {   4,   3 }, {  19,   3 }, {  -9,   4 }, {  -3,   4 },
    {   3,   4 }, {   7,   4 }, {  25,   4 }, { -13,   5 }, {  -5,   5 }, {  -2,   5 }, {   0,   5 }, {   2,   5 },
    {   5,   5 }, {   9,   5 }, {  33,   5 }, {  -8,   6 }, {  -4,   6 }, {   4,   6 }, {  13,   6 }, {  43,   6 },
    { -18,   7 }, {  -2,   7 }, {   0,   7 }, {   2,   7 }, {   7,   7 }, {  18,   7 }, { -24,   8 }, {  -6,   8 },
    { -42,   9 }, { -11,   9 }, {  -4,   9 }, {   5,   9 }, {  11,   9 }, {  23,   9 }, { -31,  10 }, {  -1,  10 },
    {   2,  10 }, { -15,  11 }, {  -8,  11 }, {   8,  11 }, {  15,  11 }, {  31,  12 }, { -21,  13 }, {  -5,  13 },
    {   5,  13 }, {  41,  13 }, {  -1,  14 }, {   1,  14 }, {  21,  14 }, { -12,  15 }, {  12,  15 }, { -39,  17 },
    { -28,  17 }, { -18,  17 }, {  -8,  17 }, {   8,  17 }, {  17,  18 }, {  -4,  19 }, {   0,  19 }, {   4,  19 },
    {  27,  19 }, {  38,  20 }, { -13,  21 }, {  12,  22 }, { -36,  23 }, { -24,  23 }, {  -8,  24 }, {   7,  24 },
    {  -3,  25 }, {   1,  25 }, {  22,  25 }, {  34,  26 }, { -18,  28 }, { -32,  29 }, {  16,  29 }, { -11,  31 },
    {   9,  32 }, {  29,  32 }, {  -4,  33 }, {   2,  33 }, { -26,  34 },
};

// Glyph edge points: a glyph is the line between two of these points, with one side of it filled
static const uint8_t SmushDecoder_aGlyph4X[16] = { 0, 1, 2, 3, 3, 3, 3, 2, 1, 0, 0, 0, 1, 2, 2, 1 };
static const uint8_t SmushDecoder_aGlyph4Y[16] = { 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 2, 1, 1, 1, 2, 2 };
static const uint8_t SmushDecoder_aGlyph8X[16] = { 0, 2, 5, 7, 7, 7, 7, 7, 7, 5, 2, 0, 0, 0, 0, 0 };
static const uint8_t SmushDecoder_aGlyph8Y[16] = { 0, 0, 0, 0, 1, 3, 4, 6, 7, 7, 7, 7, 6, 4, 3, 1 };

// VIMA (variable bit rate IMA ADPCM)
static const uint16_t SmushDecoder_aVimaStep[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,    25,    28,    31,
    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,   130,   143,
    157,   173,   190,   209,   230,   253,   279,   307,   337,   371,   408,   449,   494,   544,   598,   658,
    724,   796,   876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,
    3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static const int8_t SmushDecoder_aVimaIndex4[8] = { -1, -1, -1, -1, 1, 2, 4, 6 };
static const int8_t SmushDecoder_aVimaIndex5[16] = { -1, -1, -1, -1, -1, -1, -1, -1, 1, 1, 1, 2, 2, 4, 5, 6 };
static const int8_t SmushDecoder_aVimaIndex6[32] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1,  1,  1,  1,  1,  2,  2,  2,  2,  4,  4,  4,  5,  5,  6,  6
};
static const int8_t SmushDecoder_aVimaIndex7[64] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,
     2,  2,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  6
};

static inline uint32_t SmushDecoder_GetBE32(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline uint16_t SmushDecoder_GetLE16(const uint8_t* p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline uint32_t SmushDecoder_GetLE32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int SmushDecoder_Read(SmushDecoder* pDecoder, void* pDst, size_t size)
{
    return pDecoder->pfRead(pDecoder->pUser, pDst, size) == size;
}

static int SmushDecoder_ReadChunk(SmushDecoder* pDecoder, uint32_t* pTag, size_t* pSize)
{
    if ( pDecoder->bPendingFrame )
    {
        // The first frame, read while looking for the header chunks
        pDecoder->bPendingFrame = 0;
        *pTag  = SMUSHTAG_FRME;
        *pSize = pDecoder->pendingSize;
        return 1;
    }

    uint8_t aHeader[8];
    if ( !SmushDecoder_Read(pDecoder, aHeader, sizeof(aHeader)) )
    {
        return 0;
    }

    *pTag  = SmushDecoder_GetBE32(aHeader);
    *pSize = SmushDecoder_GetBE32(aHeader + 4);

    if ( *pSize > pDecoder->chunkCapacity )
    {
        uint8_t* pNew = (uint8_t*)realloc(pDecoder->pChunk, *pSize);
        if ( !pNew )
        {
            return -1;
        }

        pDecoder->pChunk        = pNew;
        pDecoder->chunkCapacity = *pSize;
    }

    if ( !SmushDecoder_Read(pDecoder, pDecoder->pChunk, *pSize) )
    {
        return -1;
    }

    // Chunks are padded to an even size; the last one of a file may lack the pad byte
    if ( *pSize & 1 )
    {
        uint8_t pad;
        pDecoder->pfRead(pDecoder->pUser, &pad, 1);
    }

    return 1;
}

static void SmushDecoder_ParseHeader(SmushDecoder* pDecoder, const uint8_t* pData, size_t size)
{
    if ( size >= 0x12 )
    {
        pDecoder->info.numFrames     = (int16_t)SmushDecoder_GetLE16(pData + 0x02);
        pDecoder->info.frameDuration = (int)SmushDecoder_GetLE32(pData + 0x0E);
    }
}

static void SmushDecoder_ParseFrameHeader(SmushDecoder* pDecoder, const uint8_t* pData, size_t size)
{
    size_t pos = 0;
    while ( pos + 8 <= size )
    {
        uint32_t tag     = SmushDecoder_GetBE32(pData + pos);
        size_t chunkSize = SmushDecoder_GetBE32(pData + pos + 4);
        pos += 8;
        if ( chunkSize > size - pos )
        {
            break;
        }

        if ( tag == SMUSHTAG_WAVE && chunkSize >= 4 )
        {
            pDecoder->info.audioRate = (int)SmushDecoder_GetLE32(pData + pos);
        }
        else if ( tag == SMUSHTAG_BL16 && chunkSize >= 6 && !pDecoder->info.width )
        {
            pDecoder->info.width        = (int16_t)SmushDecoder_GetLE16(pData + pos + 2);
            pDecoder->info.height       = (int16_t)SmushDecoder_GetLE16(pData + pos + 4);
            pDecoder->info.bufferHeight = (pDecoder->info.height + 7) & ~7;
        }

        pos += (chunkSize + 1) & ~(size_t)1;
    }
}

static void SmushDecoder_FreeBuffers(SmushDecoder* pDecoder)
{
    for ( size_t i = 0; i < 3; i++ )
    {
        free(pDecoder->apBufferMem[i]);
        pDecoder->apBufferMem[i] = NULL;
        pDecoder->apBuffer[i]    = NULL;
    }

    pDecoder->bufWidth     = 0;
    pDecoder->bufHeight    = 0;
    pDecoder->pLastPicture = NULL;
}

// Builds the pixel lists of the 256 glyphs of one size: glyph i * 16 + j is the line from edge point j to edge point i,
// plus all pixels on one side of it. Which side depends on the block edges the two points lie on.
static void SmushDecoder_MakeGlyphs(SmushGlyphPixels* aGlyphs, int size, const uint8_t* aX, const uint8_t* aY)
{
    enum { EDGE_TOP, EDGE_BOTTOM, EDGE_LEFT, EDGE_RIGHT, EDGE_NONE };
    enum { FILL_NONE, FILL_UP, FILL_DOWN, FILL_LEFT, FILL_RIGHT };

    for ( int i = 0; i < 16; i++ )
    {
        for ( int j = 0; j < 16; j++ )
        {
            int aEdge[2];
            const int aPoint[2] = { i, j };
            for ( int k = 0; k < 2; k++ )
            {
                int p = aPoint[k];
                if ( aY[p] == 0 )
                {
                    aEdge[k] = EDGE_TOP;
                }
                else if ( aY[p] == size - 1 )
                {
                    aEdge[k] = EDGE_BOTTOM;
                }
                else if ( aX[p] == 0 )
                {
                    aEdge[k] = EDGE_LEFT;
                }
                else
                {
                    aEdge[k] = aX[p] == size - 1 ? EDGE_RIGHT : EDGE_NONE;
                }
            }

            const int edge1 = aEdge[0];
            const int edge2 = aEdge[1];

            int fill = FILL_NONE;
            int bSideRule = 0;
            if ( (edge1 == EDGE_LEFT && edge2 == EDGE_RIGHT) || (edge2 == EDGE_LEFT && edge1 == EDGE_RIGHT) || (edge1 == EDGE_TOP && edge2 != EDGE_BOTTOM) )
            {
                fill = FILL_UP;
            }
            else if ( edge2 != EDGE_TOP )
            {
                if ( edge1 == EDGE_BOTTOM || (edge2 == EDGE_BOTTOM && edge1 != EDGE_TOP) )
                {
                    fill = FILL_DOWN;
                }
                else if ( edge2 == EDGE_BOTTOM )
                {
                    fill = FILL_RIGHT;
                }
                else
                {
                    bSideRule = 1;
                }
            }
            else
            {
                if ( edge1 != EDGE_BOTTOM )
                {
                    fill = FILL_UP;
                }
                else
                {
                    bSideRule = 1;
                }
            }

            if ( bSideRule )
            {
                if ( (edge1 == EDGE_TOP && edge2 == EDGE_BOTTOM) || (edge2 == EDGE_TOP && edge1 == EDGE_BOTTOM) || (edge1 == EDGE_RIGHT && edge2 != EDGE_LEFT) )
                {
                    fill = FILL_RIGHT;
                }
                else if ( edge2 == EDGE_RIGHT )
                {
                    fill = edge1 != EDGE_LEFT ? FILL_RIGHT : FILL_NONE;
                }
                else if ( edge1 == EDGE_LEFT || (edge2 == EDGE_LEFT && edge1 != EDGE_RIGHT) )
                {
                    fill = FILL_LEFT;
                }
            }

            uint8_t aMask[64] = { 0 };
            const int x1 = aX[i], y1 = aY[i];
            const int x2 = aX[j], y2 = aY[j];
            const int dx = abs(x1 - x2);
            const int dy = abs(y1 - y2);
            const int numSteps = dy < dx ? dx : dy;
            for ( int step = 0; step <= numSteps; step++ )
            {
                int x = x1;
                int y = y1;
                if ( numSteps > 0 )
                {
                    x = (step * x1 + (numSteps - step) * x2 + numSteps / 2) / numSteps;
                    y = (step * y1 + (numSteps - step) * y2 + numSteps / 2) / numSteps;
                }

#ifdef SMUSH_TEST_FFMPEG
                // Test builds only: FFmpeg's tables leave out the line itself when no side is filled
                if ( fill != FILL_NONE )
#endif
                aMask[y * size + x] = 1;
                switch ( fill )
                {
                    case FILL_UP:
                        for ( int row = y; row >= 0; row-- )
                        {
                            aMask[row * size + x] = 1;
                        }
                        break;

                    case FILL_DOWN:
                        for ( int row = y; row < size; row++ )
                        {
                            aMask[row * size + x] = 1;
                        }
                        break;

                    case FILL_LEFT:
                        for ( int col = x; col >= 0; col-- )
                        {
                            aMask[y * size + col] = 1;
                        }
                        break;

                    case FILL_RIGHT:
                        for ( int col = x; col < size; col++ )
                        {
                            aMask[y * size + col] = 1;
                        }
                        break;

                    default:
                        break;
                }
            }

            SmushGlyphPixels* pGlyph = &aGlyphs[i * 16 + j];
            pGlyph->numFg = 0;
            pGlyph->numBg = 0;
            for ( int p = size * size - 1; p >= 0; p-- )
            {
                if ( aMask[p] )
                {
                    pGlyph->aFg[pGlyph->numFg++] = (uint8_t)p;
                }
                else
                {
                    pGlyph->aBg[pGlyph->numBg++] = (uint8_t)p;
                }
            }
        }
    }
}

static void SmushDecoder_MakeOffsets(SmushDecoder* pDecoder, int pitch)
{
    if ( pitch == pDecoder->offsetsPitch )
    {
        return;
    }

    pDecoder->offsetsPitch = pitch;

    // The original stores the offsets as 16-bit values; keep the same wrap-around for very wide pictures
    for ( size_t i = 0; i < 245; i++ )
    {
        pDecoder->aMotionOffsets[i] = (int16_t)(SmushDecoder_aMotionVectors[i][1] * pitch + SmushDecoder_aMotionVectors[i][0]);
    }

    // The original's glyph loops always draw at least one pixel per list; an empty list draws offset 0, which is
    // never written and stays 0. Zeroing the unused entries keeps that.
    memset(pDecoder->aGlyph4, 0, sizeof(pDecoder->aGlyph4));
    memset(pDecoder->aGlyph8, 0, sizeof(pDecoder->aGlyph8));
    for ( size_t i = 0; i < 256; i++ )
    {
        const SmushGlyphPixels* pSrc4 = &pDecoder->aGlyph4Pixels[i];
        SmushGlyph4* pGlyph4 = &pDecoder->aGlyph4[i];
        pGlyph4->numFg = pSrc4->numFg;
        pGlyph4->numBg = pSrc4->numBg;
        for ( size_t k = 0; k < pSrc4->numFg; k++ )
        {
            pGlyph4->aFgOffsets[k] = (uint16_t)((pSrc4->aFg[k] >> 2) * pitch + (pSrc4->aFg[k] & 3));
        }

        for ( size_t k = 0; k < pSrc4->numBg; k++ )
        {
            pGlyph4->aBgOffsets[k] = (uint16_t)((pSrc4->aBg[k] >> 2) * pitch + (pSrc4->aBg[k] & 3));
        }

        const SmushGlyphPixels* pSrc8 = &pDecoder->aGlyph8Pixels[i];
        SmushGlyph8* pGlyph8 = &pDecoder->aGlyph8[i];
        pGlyph8->numFg = pSrc8->numFg;
        pGlyph8->numBg = pSrc8->numBg;
        for ( size_t k = 0; k < pSrc8->numFg; k++ )
        {
            pGlyph8->aFgOffsets[k] = (uint16_t)((pSrc8->aFg[k] >> 3) * pitch + (pSrc8->aFg[k] & 7));
        }

        for ( size_t k = 0; k < pSrc8->numBg; k++ )
        {
            pGlyph8->aBgOffsets[k] = (uint16_t)((pSrc8->aBg[k] >> 3) * pitch + (pSrc8->aBg[k] & 7));
        }
    }
}

static int SmushDecoder_AllocBuffers(SmushDecoder* pDecoder, int width, int height)
{
    SmushDecoder_FreeBuffers(pDecoder);

    const size_t numPixels = (size_t)width * (size_t)height;
    pDecoder->bufferGuard = BL16_MAXCOPYOFFSET + 16 * (size_t)width + 16;
    for ( size_t i = 0; i < 3; i++ )
    {
        pDecoder->apBufferMem[i] = (uint16_t*)calloc(numPixels + 2 * pDecoder->bufferGuard, sizeof(uint16_t));
        if ( !pDecoder->apBufferMem[i] )
        {
            SmushDecoder_FreeBuffers(pDecoder);
            return 0;
        }

        pDecoder->apBuffer[i] = pDecoder->apBufferMem[i] + pDecoder->bufferGuard;
    }

    pDecoder->bufWidth  = width;
    pDecoder->bufHeight = height;
    return 1;
}

static void SmushDecoder_FillPicture(uint16_t* pDst, size_t numPixels, uint16_t color)
{
    for ( size_t i = 0; i < numPixels; i++ )
    {
        pDst[i] = color;
    }
}

static inline int SmushDecoder_BlockHasData(SmushBlockContext* pCtx, size_t size)
{
    if ( (size_t)(pCtx->pEnd - pCtx->pCur) < size )
    {
        pCtx->bOverrun = 1;
        return 0;
    }

    return 1;
}

static void SmushDecoder_CopyBlock(uint16_t* pDst, const uint16_t* pSrc, ptrdiff_t pitch, int size)
{
    for ( int row = 0; row < size; row++ )
    {
        memcpy(pDst + row * pitch, pSrc + row * pitch, (size_t)size * sizeof(uint16_t));
    }
}

static void SmushDecoder_FillBlock(uint16_t* pDst, ptrdiff_t pitch, int size, uint16_t color)
{
    for ( int row = 0; row < size; row++ )
    {
        for ( int col = 0; col < size; col++ )
        {
            pDst[row * pitch + col] = color;
        }
    }
}

// Opcodes shared by all block sizes: motion copy from PREV1 (0x00-0xF5), solid fills (0xF9-0xFE) and copy from PREV2 (0xF6).
// Returns 1 if the opcode was one of them.
static int SmushDecoder_DecodeCommonOp(SmushBlockContext* pCtx, ptrdiff_t pos, int size, uint8_t opcode)
{
    uint16_t* pDst = pCtx->pPicture + pos;
    if ( opcode <= 0xF5 )
    {
        ptrdiff_t offset;
        if ( opcode == 0xF5 )
        {
            if ( !SmushDecoder_BlockHasData(pCtx, 2) )
            {
                return 1;
            }

            offset = (int16_t)SmushDecoder_GetLE16(pCtx->pCur);
            pCtx->pCur += 2;
        }
        else
        {
            offset = pCtx->aMotionOffsets[opcode];
        }

#ifdef SMUSH_TEST_FFMPEG
        if ( pos + offset < 0 || pos + offset + (size - 1) * pCtx->pitch + size > pCtx->numPixels )
        {
            SmushDecoder_numOutsideCopies++;
        }
#endif
        SmushDecoder_CopyBlock(pDst, pCtx->pPrev1 + pos + offset, pCtx->pitch, size);
        return 1;
    }

    switch ( opcode )
    {
        case 0xF6:
            SmushDecoder_CopyBlock(pDst, pCtx->pPrev2 + pos, pCtx->pitch, size);
            return 1;

        case 0xF9:
        case 0xFA:
        case 0xFB:
        case 0xFC:
            SmushDecoder_FillBlock(pDst, pCtx->pitch, size, pCtx->aFillColors[opcode - 0xF9]);
            return 1;

        case 0xFD:
            if ( SmushDecoder_BlockHasData(pCtx, 1) )
            {
                SmushDecoder_FillBlock(pDst, pCtx->pitch, size, pCtx->aCodebook[*pCtx->pCur++]);
            }
            return 1;

        case 0xFE:
            if ( SmushDecoder_BlockHasData(pCtx, 2) )
            {
                SmushDecoder_FillBlock(pDst, pCtx->pitch, size, SmushDecoder_GetLE16(pCtx->pCur));
                pCtx->pCur += 2;
            }
            return 1;

        default:
            return 0;
    }
}

// Glyph opcodes 0xF7 (two codebook colors) and 0xF8 (two direct colors): returns 0 on a data overrun
static int SmushDecoder_ReadGlyphOp(SmushBlockContext* pCtx, uint8_t opcode, uint8_t* pGlyphNum, uint16_t* pFgColor, uint16_t* pBgColor)
{
    if ( opcode == 0xF7 )
    {
        if ( !SmushDecoder_BlockHasData(pCtx, 3) )
        {
            return 0;
        }

        *pGlyphNum = pCtx->pCur[0];
        *pFgColor  = pCtx->aCodebook[pCtx->pCur[1]];
        *pBgColor  = pCtx->aCodebook[pCtx->pCur[2]];
        pCtx->pCur += 3;
        return 1;
    }

    if ( !SmushDecoder_BlockHasData(pCtx, 5) )
    {
        return 0;
    }

    *pGlyphNum = pCtx->pCur[0];
    *pFgColor  = SmushDecoder_GetLE16(pCtx->pCur + 1);
    *pBgColor  = SmushDecoder_GetLE16(pCtx->pCur + 3);
    pCtx->pCur += 5;
    return 1;
}

static void SmushDecoder_DecodeBlock2(SmushBlockContext* pCtx, ptrdiff_t pos)
{
    if ( !SmushDecoder_BlockHasData(pCtx, 1) )
    {
        return;
    }

    const uint8_t opcode = *pCtx->pCur++;
    if ( SmushDecoder_DecodeCommonOp(pCtx, pos, 2, opcode) )
    {
        return;
    }

    uint16_t* pDst = pCtx->pPicture + pos;
    const ptrdiff_t pitch = pCtx->pitch;
    if ( opcode == 0xF7 )
    {
        // Four codebook pixels
        if ( SmushDecoder_BlockHasData(pCtx, 4) )
        {
            pDst[0]         = pCtx->aCodebook[pCtx->pCur[0]];
            pDst[1]         = pCtx->aCodebook[pCtx->pCur[1]];
            pDst[pitch]     = pCtx->aCodebook[pCtx->pCur[2]];
            pDst[pitch + 1] = pCtx->aCodebook[pCtx->pCur[3]];
            pCtx->pCur += 4;
        }
    }
    else
    {
        // 0xF8, 0xFF: four direct pixels
        if ( SmushDecoder_BlockHasData(pCtx, 8) )
        {
            pDst[0]         = SmushDecoder_GetLE16(pCtx->pCur);
            pDst[1]         = SmushDecoder_GetLE16(pCtx->pCur + 2);
            pDst[pitch]     = SmushDecoder_GetLE16(pCtx->pCur + 4);
            pDst[pitch + 1] = SmushDecoder_GetLE16(pCtx->pCur + 6);
            pCtx->pCur += 8;
        }
    }
}

static void SmushDecoder_DecodeBlock4(SmushBlockContext* pCtx, ptrdiff_t pos)
{
    if ( !SmushDecoder_BlockHasData(pCtx, 1) )
    {
        return;
    }

    const uint8_t opcode = *pCtx->pCur++;
    if ( SmushDecoder_DecodeCommonOp(pCtx, pos, 4, opcode) )
    {
        return;
    }

    uint16_t* pDst = pCtx->pPicture + pos;
    const ptrdiff_t pitch = pCtx->pitch;
    if ( opcode == 0xFF )
    {
        SmushDecoder_DecodeBlock2(pCtx, pos);
        SmushDecoder_DecodeBlock2(pCtx, pos + 2);
        SmushDecoder_DecodeBlock2(pCtx, pos + 2 * pitch);
        SmushDecoder_DecodeBlock2(pCtx, pos + 2 * pitch + 2);
        return;
    }

    uint8_t glyphNum;
    uint16_t fgColor, bgColor;
    if ( SmushDecoder_ReadGlyphOp(pCtx, opcode, &glyphNum, &fgColor, &bgColor) )
    {
        // An empty pixel list still writes its first offset (see SmushDecoder_MakeOffsets)
        const SmushGlyph4* pGlyph = &pCtx->aGlyph4[glyphNum];
        size_t k = 0;
        do
        {
            pDst[pGlyph->aFgOffsets[k]] = fgColor;
        }
        while ( ++k < pGlyph->numFg );

        k = 0;
        do
        {
            pDst[pGlyph->aBgOffsets[k]] = bgColor;
        }
        while ( ++k < pGlyph->numBg );
    }
}

static void SmushDecoder_DecodeBlock8(SmushBlockContext* pCtx, ptrdiff_t pos)
{
    if ( !SmushDecoder_BlockHasData(pCtx, 1) )
    {
        return;
    }

    const uint8_t opcode = *pCtx->pCur++;
    if ( SmushDecoder_DecodeCommonOp(pCtx, pos, 8, opcode) )
    {
        return;
    }

    uint16_t* pDst = pCtx->pPicture + pos;
    const ptrdiff_t pitch = pCtx->pitch;
    if ( opcode == 0xFF )
    {
        SmushDecoder_DecodeBlock4(pCtx, pos);
        SmushDecoder_DecodeBlock4(pCtx, pos + 4);
        SmushDecoder_DecodeBlock4(pCtx, pos + 4 * pitch);
        SmushDecoder_DecodeBlock4(pCtx, pos + 4 * pitch + 4);
        return;
    }

    uint8_t glyphNum;
    uint16_t fgColor, bgColor;
    if ( SmushDecoder_ReadGlyphOp(pCtx, opcode, &glyphNum, &fgColor, &bgColor) )
    {
        const SmushGlyph8* pGlyph = &pCtx->aGlyph8[glyphNum];
        size_t k = 0;
        do
        {
            pDst[pGlyph->aFgOffsets[k]] = fgColor;
        }
        while ( ++k < pGlyph->numFg );

        k = 0;
        do
        {
            pDst[pGlyph->aBgOffsets[k]] = bgColor;
        }
        while ( ++k < pGlyph->numBg );
    }
}

// Codec 2: 8x8 blocks, each a motion copy, a fill, a glyph or split into 4x4 and then 2x2 blocks
static void SmushDecoder_DecodeBlocky16(SmushDecoder* pDecoder, const uint8_t* pHeader, const uint8_t* pData, const uint8_t* pEnd, int width, int height)
{
    SmushBlockContext ctx;
    ctx.pCur           = pData;
    ctx.pEnd           = pEnd;
    ctx.bOverrun       = 0;
    ctx.pitch          = width;
    ctx.numPixels      = (ptrdiff_t)width * pDecoder->bufHeight;
    ctx.pPicture       = pDecoder->apBuffer[SMUSHBUF_CUR];
    ctx.pPrev1         = pDecoder->apBuffer[SMUSHBUF_PREV1];
    ctx.pPrev2         = pDecoder->apBuffer[SMUSHBUF_PREV2];
    ctx.aMotionOffsets = pDecoder->aMotionOffsets;
    ctx.aGlyph4        = pDecoder->aGlyph4;
    ctx.aGlyph8        = pDecoder->aGlyph8;
    for ( size_t i = 0; i < 4; i++ )
    {
        ctx.aFillColors[i] = SmushDecoder_GetLE16(pHeader + BL16_FILLCOLORS + 2 * i);
    }

    for ( size_t i = 0; i < 256; i++ )
    {
        ctx.aCodebook[i] = SmushDecoder_GetLE16(pHeader + BL16_CODEBOOK + 2 * i);
    }

    const int numBlockRows = (height + 7) / 8;
    const int numBlockCols = (width + 7) / 8;
    for ( int blockRow = 0; blockRow < numBlockRows && !ctx.bOverrun; blockRow++ )
    {
        for ( int blockCol = 0; blockCol < numBlockCols && !ctx.bOverrun; blockCol++ )
        {
            SmushDecoder_DecodeBlock8(&ctx, blockRow * 8 * ctx.pitch + blockCol * 8);
        }
    }
}

// Byte-wise RLE: a control byte n gives a run of (n >> 1) + 1 bytes, repeated (n & 1) or literal
static void SmushDecoder_DecodeRle(uint8_t* pDst, size_t dstSize, const uint8_t* pSrc, const uint8_t* pEnd)
{
    size_t pos = 0;
    while ( pos < dstSize && pSrc < pEnd )
    {
        const uint8_t control = *pSrc++;
        size_t runLength = (size_t)(control >> 1) + 1;
        if ( runLength > dstSize - pos )
        {
            runLength = dstSize - pos;
        }

        if ( control & 1 )
        {
            if ( pSrc >= pEnd )
            {
                break;
            }

            memset(pDst + pos, *pSrc++, runLength);
        }
        else
        {
            if ( runLength > (size_t)(pEnd - pSrc) )
            {
                runLength = (size_t)(pEnd - pSrc);
            }

            memcpy(pDst + pos, pSrc, runLength);
            pSrc += runLength;
        }

        pos += runLength;
    }
}

// Decodes one Bl16 chunk; returns the picture to show (NULL if none) or sets *pbError
static uint16_t* SmushDecoder_DecodeBl16(SmushDecoder* pDecoder, const uint8_t* pData, size_t size, int bSkipDisposable, int* pbError)
{
    if ( size < BL16_HEADERSIZE )
    {
        *pbError = 1;
        return NULL;
    }

    const int width    = (int16_t)SmushDecoder_GetLE16(pData + BL16_WIDTH);
    const int height   = (int16_t)SmushDecoder_GetLE16(pData + BL16_HEIGHT);
    const int seqNum   = (int16_t)SmushDecoder_GetLE16(pData + BL16_SEQNUM);
    const uint8_t codec    = pData[BL16_CODEC];
    const uint8_t rotation = pData[BL16_ROTATION];
    if ( width <= 0 || height <= 0 )
    {
        *pbError = 1;
        return NULL;
    }

    const int bufHeight = (height + 7) & ~7;
    if ( !pDecoder->apBuffer[0] || width != pDecoder->bufWidth || bufHeight != pDecoder->bufHeight )
    {
        if ( !SmushDecoder_AllocBuffers(pDecoder, width, bufHeight) )
        {
            *pbError = 1;
            return NULL;
        }

        pDecoder->info.width        = width;
        pDecoder->info.height       = height;
        pDecoder->info.bufferHeight = bufHeight;
    }

    const size_t numPixels = (size_t)width * (size_t)bufHeight;
    if ( seqNum == 0 )
    {
        // Key frame: both reference pictures start from the clear color
        SmushDecoder_MakeOffsets(pDecoder, width);
        const uint16_t clearColor = SmushDecoder_GetLE16(pData + BL16_CLEARCOLOR);
        SmushDecoder_FillPicture(pDecoder->apBuffer[SMUSHBUF_PREV2], numPixels, clearColor);
        SmushDecoder_FillPicture(pDecoder->apBuffer[SMUSHBUF_PREV1], numPixels, clearColor);
        pDecoder->lastSeqNum = -1;
    }

    const uint8_t* pCodecData = pData + BL16_HEADERSIZE;
    const uint8_t* pEnd       = pData + size;
    const size_t codecSize    = size - BL16_HEADERSIZE;
    const size_t pictureSize  = (size_t)width * (size_t)height * sizeof(uint16_t);
    uint16_t* pCur            = pDecoder->apBuffer[SMUSHBUF_CUR];
    const int bInSequence     = seqNum == pDecoder->lastSeqNum + 1;

    // Pictures that don't become a reference can be dropped when the player is late
    if ( bSkipDisposable && rotation == 0 && (codec == 2 || codec == 3 || codec == 4) )
    {
        pDecoder->lastSeqNum = seqNum;
        return NULL;
    }

    switch ( codec )
    {
        case 0: // raw picture
            memcpy(pCur, pCodecData, codecSize < pictureSize ? codecSize : pictureSize);
            pDecoder->pLastPicture = pCur;
            break;

        case 2: // blocky16, needs the previous frames
            if ( bInSequence )
            {
                SmushDecoder_DecodeBlocky16(pDecoder, pData, pCodecData, pEnd, width, height);
                pDecoder->pLastPicture = pCur;
            }
            break;

        case 3: // repeat PREV1
            memcpy(pCur, pDecoder->apBuffer[SMUSHBUF_PREV1], pictureSize);
            pDecoder->pLastPicture = pCur;
            break;

        case 4: // repeat PREV2
            memcpy(pCur, pDecoder->apBuffer[SMUSHBUF_PREV2], pictureSize);
            pDecoder->pLastPicture = pCur;
            break;

        case 5: // RLE picture
        {
            size_t rleSize = SmushDecoder_GetLE32(pData + BL16_RLESIZE);
            if ( rleSize > numPixels * sizeof(uint16_t) )
            {
                rleSize = numPixels * sizeof(uint16_t);
            }

            SmushDecoder_DecodeRle((uint8_t*)pCur, rleSize, pCodecData, pEnd);
            pDecoder->pLastPicture = pCur;
            break;
        }

        default:
            // Codecs 1 and 6-8 (half-resolution and codebook pictures) don't occur in the game's movies
            break;
    }

    uint16_t* pPicture = pDecoder->pLastPicture;

    if ( bInSequence )
    {
        uint16_t* pPrev2 = pDecoder->apBuffer[SMUSHBUF_PREV2];
        uint16_t* pPrev1 = pDecoder->apBuffer[SMUSHBUF_PREV1];
        uint16_t* pPrev2Mem = pDecoder->apBufferMem[SMUSHBUF_PREV2];
        uint16_t* pPrev1Mem = pDecoder->apBufferMem[SMUSHBUF_PREV1];
        uint16_t* pCurMem   = pDecoder->apBufferMem[SMUSHBUF_CUR];
        if ( rotation == 1 )
        {
            pDecoder->apBuffer[SMUSHBUF_PREV1]    = pCur;
            pDecoder->apBuffer[SMUSHBUF_CUR]      = pPrev1;
            pDecoder->apBufferMem[SMUSHBUF_PREV1] = pCurMem;
            pDecoder->apBufferMem[SMUSHBUF_CUR]   = pPrev1Mem;
        }
        else if ( rotation == 2 )
        {
            pDecoder->apBuffer[SMUSHBUF_PREV2]    = pPrev1;
            pDecoder->apBuffer[SMUSHBUF_PREV1]    = pCur;
            pDecoder->apBuffer[SMUSHBUF_CUR]      = pPrev2;
            pDecoder->apBufferMem[SMUSHBUF_PREV2] = pPrev1Mem;
            pDecoder->apBufferMem[SMUSHBUF_PREV1] = pCurMem;
            pDecoder->apBufferMem[SMUSHBUF_CUR]   = pPrev2Mem;
        }
    }

    pDecoder->lastSeqNum = seqNum;
    return pPicture;
}

static void SmushDecoder_InitVima(SmushDecoder* pDecoder)
{
    for ( size_t stepIdx = 0; stepIdx < 89; stepIdx++ )
    {
        for ( unsigned int code = 0; code < 64; code++ )
        {
            // Sum of step >> k for the bits of the 6-bit code, highest bit first
            unsigned int step = SmushDecoder_aVimaStep[stepIdx];
            unsigned int delta = 0;
            for ( unsigned int mask = 0x20; mask; mask >>= 1 )
            {
                if ( code & mask )
                {
                    delta += step;
                }

                step >>= 1;
            }

            pDecoder->aVimaDelta[stepIdx][code] = (uint16_t)delta;
        }
    }
}

static inline uint8_t SmushDecoder_GetBits(SmushBitReader* pReader)
{
    return pReader->pCur < pReader->pEnd ? *pReader->pCur++ : 0;
}

static inline int SmushDecoder_GetVimaBitCount(int stepIdx)
{
    return stepIdx < 45 ? 4 : stepIdx < 59 ? 5 : stepIdx < 74 ? 6 : 7;
}

static const int8_t* SmushDecoder_GetVimaIndexTable(int numBits)
{
    switch ( numBits )
    {
        case 4: return SmushDecoder_aVimaIndex4;
        case 5: return SmushDecoder_aVimaIndex5;
        case 6: return SmushDecoder_aVimaIndex6;
        default: return SmushDecoder_aVimaIndex7;
    }
}

// Decodes numSamples per channel; the channels are stored one after the other in a single bit stream
static void SmushDecoder_DecodeVima(const SmushDecoder* pDecoder, int16_t* pDst, SmushBitReader* pReader, size_t numSamples, int numChannels, const int* aStepIdx, const int* aPredictor)
{
    unsigned int window = (unsigned int)SmushDecoder_GetBits(pReader) << 8;
    window |= SmushDecoder_GetBits(pReader);
    int bitPos = 0;

    for ( int channel = 0; channel < numChannels; channel++ )
    {
        int stepIdx = aStepIdx[channel];
        int predictor = aPredictor[channel];
        int16_t* pOut = pDst + channel;

        for ( size_t i = 0; i < numSamples; i++ )
        {
            const int numBits = SmushDecoder_GetVimaBitCount(stepIdx);
            const unsigned int signBit = 1u << (numBits - 1);
            const unsigned int dataMask = signBit - 1;

            bitPos += numBits;
            unsigned int code = (window >> (16 - bitPos)) & (signBit | dataMask);
            if ( bitPos > 7 )
            {
                window = ((window & 0xFF) << 8) | SmushDecoder_GetBits(pReader);
                bitPos -= 8;
            }

            const unsigned int bNegative = code & signBit;
            code &= dataMask;

            if ( code == dataMask )
            {
                // Escape: the next 16 bits are the sample itself
                const unsigned int next = SmushDecoder_GetBits(pReader);
                const unsigned int high = (window << bitPos) & 0xFF00;
                const unsigned int low  = ((((window & 0xFF) << 8) | next) >> (8 - bitPos)) & 0xFF;
                predictor = (int16_t)(high | low);
                window = (next << 8) | SmushDecoder_GetBits(pReader);
            }
            else
            {
                int delta = pDecoder->aVimaDelta[stepIdx][code << (7 - numBits)];
                if ( code )
                {
                    delta += SmushDecoder_aVimaStep[stepIdx] >> (numBits - 1);
                }

                if ( bNegative )
                {
                    delta = -delta;
                }

                predictor += delta;
                if ( predictor < -32768 )
                {
                    predictor = -32768;
                }
                else if ( predictor > 32767 )
                {
                    predictor = 32767;
                }
            }

            *pOut = (int16_t)predictor;
            pOut += numChannels;

            stepIdx += SmushDecoder_GetVimaIndexTable(numBits)[code];
            if ( stepIdx < 0 )
            {
                stepIdx = 0;
            }
            else if ( stepIdx > 88 )
            {
                stepIdx = 88;
            }
        }
    }
}

// Wave chunk: [-1, track id,] sample count, then per channel the start step index and predictor, then VIMA data
static int SmushDecoder_DecodeWave(SmushDecoder* pDecoder, const uint8_t* pData, size_t size, SmushFrame* pFrame)
{
    const uint8_t* pCur = pData;
    const uint8_t* pEnd = pData + size;
    if ( size < 4 )
    {
        return 1;
    }

    int32_t numSamples = (int32_t)SmushDecoder_GetBE32(pCur);
    pCur += 4;
    if ( numSamples < 0 )
    {
        if ( pEnd - pCur < 8 )
        {
            return 1;
        }

        numSamples = (int32_t)SmushDecoder_GetBE32(pCur + 4);
        pCur += 8;
    }

    if ( numSamples <= 0 || pEnd - pCur < 3 )
    {
        return 1;
    }

    int numChannels = 1;
    int aStepIdx[2] = { 0 };
    int aPredictor[2] = { 0 };

    int8_t firstIdx = (int8_t)*pCur++;
    if ( firstIdx < 0 )
    {
        firstIdx = (int8_t)~firstIdx;
        numChannels = 2;
    }

    aStepIdx[0] = firstIdx;
    aPredictor[0] = (int16_t)((pCur[0] << 8) | pCur[1]);
    pCur += 2;
    if ( numChannels > 1 )
    {
        if ( pEnd - pCur < 3 )
        {
            return 1;
        }

        aStepIdx[1] = (int8_t)*pCur++;
        aPredictor[1] = (int16_t)((pCur[0] << 8) | pCur[1]);
        pCur += 2;
    }

    // Frames with several audio chunks append them; a channel count change starts over
    if ( pFrame->numAudioSamples && pFrame->numAudioChannels != numChannels )
    {
        pFrame->numAudioSamples = 0;
    }

    const size_t needed = (pFrame->numAudioSamples + (size_t)numSamples) * (size_t)numChannels;
    if ( needed > pDecoder->pcmCapacity )
    {
        int16_t* pNew = (int16_t*)realloc(pDecoder->pPcm, needed * sizeof(int16_t));
        if ( !pNew )
        {
            return 0;
        }

        pDecoder->pPcm = pNew;
        pDecoder->pcmCapacity = needed;
    }

    SmushBitReader reader = { pCur, pEnd };
    SmushDecoder_DecodeVima(pDecoder, pDecoder->pPcm + pFrame->numAudioSamples * (size_t)numChannels, &reader, (size_t)numSamples, numChannels, aStepIdx, aPredictor);

    pFrame->numAudioSamples += (size_t)numSamples;
    pFrame->numAudioChannels = numChannels;
    pFrame->pAudio = pDecoder->pPcm;
    return 1;
}

SmushDecoder* SmushDecoder_Open(SmushReadFunc pfRead, void* pUser)
{
    SmushDecoder* pDecoder = (SmushDecoder*)calloc(1, sizeof(SmushDecoder));
    if ( !pDecoder )
    {
        return NULL;
    }

    pDecoder->pfRead       = pfRead;
    pDecoder->pUser        = pUser;
    pDecoder->lastSeqNum   = -1;
    pDecoder->offsetsPitch = -1;
    SmushDecoder_MakeGlyphs(pDecoder->aGlyph4Pixels, 4, SmushDecoder_aGlyph4X, SmushDecoder_aGlyph4Y);
    SmushDecoder_MakeGlyphs(pDecoder->aGlyph8Pixels, 8, SmushDecoder_aGlyph8X, SmushDecoder_aGlyph8Y);
    SmushDecoder_InitVima(pDecoder);

    uint8_t aFileHeader[8];
    if ( !SmushDecoder_Read(pDecoder, aFileHeader, sizeof(aFileHeader)) || SmushDecoder_GetBE32(aFileHeader) != SMUSHTAG_SANM )
    {
        SmushDecoder_Close(pDecoder);
        return NULL;
    }

    // Header chunks come before the first frame
    for ( ;; )
    {
        uint32_t tag;
        size_t size;
        if ( SmushDecoder_ReadChunk(pDecoder, &tag, &size) <= 0 )
        {
            SmushDecoder_Close(pDecoder);
            return NULL;
        }

        if ( tag == SMUSHTAG_SHDR )
        {
            SmushDecoder_ParseHeader(pDecoder, pDecoder->pChunk, size);
        }
        else if ( tag == SMUSHTAG_FLHD )
        {
            SmushDecoder_ParseFrameHeader(pDecoder, pDecoder->pChunk, size);
            break;
        }
        else if ( tag == SMUSHTAG_FRME )
        {
            pDecoder->bPendingFrame = 1;
            pDecoder->pendingSize   = size;
            break;
        }
    }

    return pDecoder;
}

void SmushDecoder_Close(SmushDecoder* pDecoder)
{
    if ( !pDecoder )
    {
        return;
    }

    SmushDecoder_FreeBuffers(pDecoder);
    free(pDecoder->pChunk);
    free(pDecoder->pPcm);
    free(pDecoder);
}

const SmushInfo* SmushDecoder_GetInfo(const SmushDecoder* pDecoder)
{
    return &pDecoder->info;
}

int SmushDecoder_DecodeFrame(SmushDecoder* pDecoder, int bSkipDisposable, SmushFrame* pFrame)
{
    memset(pFrame, 0, sizeof(SmushFrame));

    uint32_t tag;
    size_t size;
    do
    {
        int result = SmushDecoder_ReadChunk(pDecoder, &tag, &size);
        if ( result <= 0 )
        {
            return result;
        }
    }
    while ( tag != SMUSHTAG_FRME );

    pFrame->frameNum = pDecoder->frameNum++;

    const uint8_t* pData = pDecoder->pChunk;
    size_t pos = 0;
    while ( pos + 8 <= size )
    {
        const uint32_t subTag  = SmushDecoder_GetBE32(pData + pos);
        const size_t subSize   = SmushDecoder_GetBE32(pData + pos + 4);
        pos += 8;
        if ( subSize > size - pos )
        {
            return -1;
        }

        if ( subTag == SMUSHTAG_BL16 )
        {
            int bError = 0;
            uint16_t* pPicture = SmushDecoder_DecodeBl16(pDecoder, pData + pos, subSize, bSkipDisposable, &bError);
            if ( bError )
            {
                return -1;
            }

            if ( pPicture )
            {
                pFrame->pPixels = pPicture;
            }
        }
        else if ( subTag == SMUSHTAG_WAVE )
        {
            if ( !SmushDecoder_DecodeWave(pDecoder, pData + pos, subSize, pFrame) )
            {
                return -1;
            }
        }

        pos += (subSize + 1) & ~(size_t)1;
    }

    return 1;
}
