// Everything except stb_image was made by Felipe Alfonso (@bitnenfer)
//
// This is a crappy software but it does what it needed from it. It just 
// transform images (PNG, JPG, TGA, etc.) to DMG encoded tiles. It wont check
// for repeating tiles. Maybe in the future I can add that.
//
// Compile with GCC. gcc img2dmg.c -o img2dmg
// and it's done.
//
// Usage:
//  img2dmg my-image.png ./path/to/save/to/ output_name
//
// The program will generate 2 files:
// - output_name.bin which will contain the tile set data (pixels)
// - output_name.inc which will contain tile set metadata. (size in bytes of output_name.bin)
//
// For example if we save the data as "tile".
// A simple routine to load the generated data into VRAM could be (using RGBDS):
// ** NOTE: This will only work if the LCD is off. You'll need a safe solution for
//          loading when the LCD is on.
// 
//     include "tile.inc"
// 
// main:
//     ; turn off lcd
//     ld a,[LCDC]
//     res 7,a
//     ld [LCDC],a
//     ; set background palette
//     ld a,%11100100
//     ld [BGP],a
//     ld de, tile_data
//     ld hl, VRAM_TILE_START
//     ld bc, tile_size
//  
//     ; We just loop through
//     ; every byte in tile.bin
//     ; and store it in VRAM tile address.
// .memcpy_vram:
//     ld a,[de]
//     ld [hl],a
//     inc hl
//     inc de
//     dec bc
//     ld a,c
//     cp $00
//     jr nz,.memcpy_vram
//     ld a,b
//     cp $00
//     jr nz,.memcpy_vram
//
//     ; turn on lcd
//     ld a,[LCDC]
//     set 7,a
//     ld [LCDC],a
//
//     ; just loop forever
// loop:
//     jr loop 
//
//     ; include out static data after
//     ; out program.
//  tile_data:
//     incbin "tile.bin"
//

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // <-- You need to get stb_image.h (https://github.com/nothings/stb/blob/master/stb_image.h)

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define DEFAULT_CHANNEL_COUNT 4
#define TILE_WIDTH 8
#define TILE_HEIGHT 8

int main (int argc, const char* argv[]) {

    if (argc <= 3) {
        printf("Missing image input.\nUse:\n\timg2dmg my-image.png ./output/path/ output_name\n");
        return 0;
    }

    char bigTmpBuffer[512]; // NOTE: If your output path is bigger than 512 bytes then change this value.
    const char* pImageName = argv[1];
    const char* pOutputPath = argv[2];
    const char* pOutputName = argv[3];

    int width, height, channels;
    unsigned char* pImageData;

    sprintf(bigTmpBuffer, "%s\\%s.bin", pOutputPath, pOutputName);

    FILE* pTileBinFile = fopen(bigTmpBuffer, "wb");

    if (pTileBinFile == NULL) {
        fprintf(stderr, "Failed to open output file %s\n", bigTmpBuffer);
        return 1;
    }

    sprintf(bigTmpBuffer, "%s\\%s.inc", pOutputPath, pOutputName);

    FILE* pTileDescFile = fopen(bigTmpBuffer, "wb");

    if (pTileDescFile == NULL) {
        fprintf(stderr, "Failed to open output file %s\n", bigTmpBuffer);
        return 1;
    }

    pImageData = stbi_load(pImageName, &width, &height, &channels, DEFAULT_CHANNEL_COUNT);

    if (pImageData != NULL) {

        if ((width % 8) != 0 || (height % 8) != 0) 
        {
            fprintf(stderr, "Image must have dimension that are multiple of 8.\n");
            return 1;
        }

        int byteSize = width * height * DEFAULT_CHANNEL_COUNT;
        int outputByteSize = ((width / 8) * (height / 8)) * 16;
        int byteCount = 0;
        unsigned char* pTileData = (unsigned char*)malloc(outputByteSize);
        unsigned char* pTileDataCurr = pTileData;

        for (int tileY = 0; tileY < height; tileY += TILE_HEIGHT) {
            for (int tileX = 0; tileX < width; tileX += TILE_WIDTH) {
                for (int y = 0; y < TILE_HEIGHT; ++y) {
                    for (int x = 0; x < TILE_WIDTH; x += TILE_WIDTH) {

                        unsigned char paletteIndices[TILE_WIDTH];
                        unsigned char topByte = 0;
                        unsigned char bottomByte = 0;

                        // Process a row of pixels and store their palette indices.
                        for (int i = 0; i < TILE_WIDTH; ++i) {
                            int index = ((tileY + y) * width + (tileX + x + i)) * DEFAULT_CHANNEL_COUNT;

                            assert(index < byteSize); // Don't overflow the pixel array!!

                            float r = (float)pImageData[index] / 255.0f;
                            float g = (float)pImageData[index + 1] / 255.0f;
                            float b = (float)pImageData[index + 2] / 255.0f;

                            // Calculate luminance.
                            float lum = r * 0.3f + g * 0.59f + b * 0.11f;

                            // Map luminance to palette index. We'll assume palette goes 0 = "white" to 3 = "black"
                            unsigned char paletteIndex = 0;
                            if (lum < 0.33f) {
                                if (lum < 0.25f) {
                                    paletteIndex = 3;
                                } else {
                                    paletteIndex = 2;
                                }
                            } else if (lum < 0.66f) {
                                if (lum < 0.50f) {
                                    paletteIndex = 2;
                                } else {
                                    paletteIndex = 1;
                                }
                            } else if (lum <= 1.0f) {
                                if (lum < 0.76f) {
                                    paletteIndex = 1;
                                } else {
                                    paletteIndex = 0;
                                }
                            } else {
                                paletteIndex = 0;
                            }

                            paletteIndices[(TILE_WIDTH - 1) - i] = paletteIndex;
                        }

                        // Encode palette index to DMG tile format.
                        // 2 bytes make 1 horizontal line of a 8x8 tile

                        // Top Byte
                        for (int i = 0; i < TILE_WIDTH; ++i) {
                            unsigned char topBit = ((paletteIndices[i] >> 1) & 1) << i;
                            topByte = (topByte | topBit);
                        }

                        // Bottom Byte
                        for (int i = 0; i < TILE_WIDTH; ++i) {
                            unsigned char bottomBit = ((paletteIndices[i]) & 1) << i;
                            bottomByte = (bottomByte | bottomBit);
                        }

                        (*pTileDataCurr++) = topByte;
                        (*pTileDataCurr++) = bottomByte;
                    }
                }
            }
        }

        int bytesWritten = sprintf(bigTmpBuffer, "%s_size EQU $%04X\n", pOutputName, outputByteSize);
        fwrite(bigTmpBuffer, 1, bytesWritten, pTileDescFile);
        fwrite(pTileData, 1, outputByteSize, pTileBinFile);
        fclose(pTileDescFile);
        fclose(pTileBinFile);
        free(pTileData);
        stbi_image_free(pImageData);
    } else {
        fprintf(stderr, "Failed to open image.\n");
        return 1;
    }
    return 0;
}