/*
    Copyright (C) 1995-2011 Jan Eric Kyprianidis <www.kyprianidis.com>
    All rights reserved.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "tga.h"


/*--Descriptor flags---------------*/
#define ds_BottomLeft  0x00
#define ds_BottomRight 0x01
#define ds_TopLeft     0x02
#define ds_TopRight    0x03

/*--Color map types----------------*/
#define cm_NoColorMap  0
#define cm_ColorMap    1

/*--Image data types---------------*/
#define im_NoImageData    0
#define im_ColMapImage    1
#define im_RgbImage       2
#define im_MonoImage      3
#define im_ColMapImageRLE 9
#define im_RgbImageRLE    10
#define im_MonoImageRLE   11


static unsigned short fgetword(FILE *f) {
    unsigned char b[2];
    fread(b, 2, 1, f);
    return ((unsigned short)b[1] << 8) | (unsigned short)b[0];
}


static void  fputword(unsigned short s, FILE *f) {
    unsigned char b[2];
    b[0] = (unsigned char)(s & 0xff);
    b[1] = (unsigned char)((s >> 8) & 0xff);
    fwrite(b, 2, 1, f);
}


int tga_load(const char *name, unsigned char **buffer, int *width, int *height) {
    FILE *f;
    int idLen;
    int colorMapType;
    int imageType;
    int firstColor;
    int colorMapLen;
    int colorMapBits;
    int xOrgin;
    int yOrgin;
    int iw;
    int ih;
    int bitsPerPix;
    int descriptor;
    int i, j, k;
    unsigned char *p;
    unsigned char *q;
    unsigned char *pp;
    int bb;
    int cp[4];
    int rle;
    int n;
    int c;
    int ix, iy;
    unsigned char colorMap[256][3];
    unsigned char *data;

    data = NULL;
    if ((f = fopen(name, "rb")) == NULL) 
        return 0;

    idLen = fgetc(f);
    colorMapType = fgetc(f);
    imageType = fgetc(f);
    firstColor = fgetword(f);
    colorMapLen = fgetword(f);
    colorMapBits = fgetc(f);
    xOrgin = fgetword(f);
    yOrgin = fgetword(f);
    iw = fgetword(f);
    ih = fgetword(f);
    bitsPerPix = fgetc(f);
    descriptor = fgetc(f);

    switch(imageType) {
        case im_MonoImage:
        case im_ColMapImage:
        case im_RgbImage:
        case im_MonoImageRLE:
        case im_ColMapImageRLE:
        case im_RgbImageRLE:
            break;
        default:
            goto error;
    }
    switch (bitsPerPix) {
        case 8:
        case 24:
        case 32:
            bb = bitsPerPix / 8;
            break;
        default:
            goto error;
    }

    if (idLen)
        fseek(f, idLen, SEEK_CUR);
    *width = iw;
    *height = ih;

    if (colorMapType == cm_ColorMap) {
        memset(colorMap, 0, sizeof(colorMap));
        if ((colorMapBits != 24) || (colorMapLen + firstColor > 256)) {
            goto error;
        }
        for (i = 0, pp = colorMap[firstColor]; i < colorMapLen; i++) {
            *pp++ = fgetc(f);
            *pp++ = fgetc(f);
            *pp++ = fgetc(f);
        }
    } else {
        for (i = 0; i < 256; ++i)
            colorMap[i][0] = colorMap[i][1] = colorMap[i][2] = i;
    }

    if ((data = malloc(iw * bb * ih)) == NULL) {
        goto error;
    }

    if ((imageType == im_MonoImage) || (imageType == im_ColMapImage) || (imageType == im_RgbImage)) {
        fread(data, ih, iw*bb, f);
    } else {
        p = data;
        n = 0;
        for (j = 0; j < ih; ++j) {
            for (i = 0; i < iw; ++i, p += bb) {
                if (n == 0) {
                    c = fgetc(f);
                    n = (c & 0x7F) + 1;
                    if (c&0x80) {
                        for (k = 0; k < bb; k++) cp[k] = fgetc(f);
                        rle = 1;
                    } else {
                        rle = 0;
                    }
                }
                if (rle)
                    for (k = 0; k < bb; k++) p[k] = cp[k];
                else
                    for (k = 0; k < bb; k++) p[k] = fgetc(f);
                n--;
            }
        }
    }
    if (ferror(f)) {
        goto error;
    }
    fclose(f);

    *buffer = (unsigned char*)malloc(iw * ih * 4);
    if (!*buffer)
        goto error;

    p = *buffer;
    switch ((descriptor >> 4) & 0x3) {
        case ds_BottomLeft: 
            ix = bb;
            iy = -2 * iw * bb;
            q = data + iw * bb * (ih - 1);
            break;
        case ds_BottomRight:
            ix = -bb;
            iy = 0;
            q = data + (iw - 1) * bb * (ih);
            break;
        case ds_TopLeft:
            ix = bb;
            iy = 0;
            q = data;
            break;
        case ds_TopRight:
            ix = -bb;
            iy = 2 * iw * bb;
            q = data + (iw - 1) * bb;
            break;
    }

    switch (bitsPerPix) {
        case 8: {
            for (j = 0; j < ih; ++j) {
                for (i = 0; i < iw; ++i) {
                    for (k = 0; k < 3; ++k) {
                        p[k] = colorMap[*q][k];
                    }
                    p[3] = 0xff;
                    p += 4;
                    q += ix;
                }
                q += iy;
            }
            break;
        }

        case 24: {
            for (j = 0; j < ih; ++j) {
                for (i = 0; i < iw; ++i) {
                    p[0] = q[0];
                    p[1] = q[1];
                    p[2] = q[2];
                    p[3] = 0xff;
                    p += 4;
                    q += ix;
                }
                q += iy;
            }
            break;
        }

        case 32: {
            for (j = 0; j < ih; ++j) {
                for (i = 0; i < iw; ++i) {
                    p[0] = q[0];
                    p[1] = q[1];
                    p[2] = q[2];
                    p[3] = q[3];
                    p += 4;
                    q += ix;
                }
                q += iy;
            }
            break;
        }
    }

    free(data);
    return 1;

error:
    if (data) free(data);
    if (f) fclose(f);
    *buffer = NULL;
    *width = 0;
    *height = 0;
    return 0;
}


int tga_save(const char *name, unsigned char *buffer, unsigned char *colorMap, int width, int height, int bits) {
    FILE *f;
    int colorMapType = 0;
    int imageType;
    int colorMapLen = 0;
    int colorMapBits = 0;
    int bitsPerPix;
    int i;
    int bb;
    unsigned char *pp;

    if ((f = fopen(name, "wb+")) == NULL) 
        return 0;

    switch (bits) {
        case 8:
            imageType = im_ColMapImage;
            if (colorMap != NULL) {
                colorMapType = cm_ColorMap;
                colorMapLen = 256;
                colorMapBits = 24;
            }
            bitsPerPix = 8;
            bb = 1;
            break;
        case 16:
            imageType = im_RgbImage;
            bitsPerPix = 16;
            bb = 2;
            break;
        case 24:
            imageType = im_RgbImage;
            bitsPerPix = 24;
            bb = 3;
            break;
        case 32:
            imageType = im_RgbImage;
            bitsPerPix = 32;
            bb = 4;
            break;
        default:
            fclose(f);
            return 0;
    }

    fputc(0, f);
    fputc(colorMapType, f);
    fputc(imageType, f);
    fputword(0, f);
    fputword(colorMapLen, f);
    fputc(colorMapBits, f);
    fputword(0, f);
    fputword(0, f);
    fputword(width, f);
    fputword(height, f);
    fputc(bitsPerPix, f);
    fputc(ds_TopLeft << 4, f);

    if (colorMapType == cm_ColorMap) {
        for (i = 0, pp = colorMap; i < 256; i++, pp += 3) {
            fputc(pp[0], f);
            fputc(pp[1], f);
            fputc(pp[2], f);
        }
    }

    fwrite(buffer, width*bb, height, f);
    if (ferror(f)) {
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}
