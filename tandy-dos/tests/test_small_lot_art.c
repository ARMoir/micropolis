/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../small_lot_art.h"
#include "../zoning.h"
#include <assert.h>
#include <stdio.h>

static void fixture(unsigned int length, int bad_header)
{
    static const unsigned char header[8] = {'S','2','L','T',1,8,12,32};
    unsigned int i;
    FILE *f = fopen("lot_test.dat", "wb");
    assert(f != NULL);
    for (i=0U; i<8U; ++i)
        assert(fputc(bad_header && i==4U ? 9 : header[i],f)!=EOF);
    for (i=0U; i<length; ++i)
        assert(fputc((int)(i/32U),f)!=EOF);
    assert(fclose(f)==0);
}

int main(void)
{
    unsigned int x,y,kind,stage,id,seen=0U;
    const unsigned char *p;
    assert(!small_lot_art_load("missing-lot-art.dat"));
    assert(small_lot_art_pixels(0,ZONE_COM,1,0,0)==NULL);
    fixture(SMALL_LOT_ART_BYTES,0);
    assert(small_lot_art_load("lot_test.dat"));
    for (kind=ZONE_COM; kind<=ZONE_IND; ++kind)
        for (stage=1U; stage<=3U; ++stage)
            for (x=0U; x<120U; ++x)
                for (y=0U; y<100U; ++y) {
                    p=small_lot_art_pixels(0,kind,stage,x,y);
                    assert(p!=NULL && p[0]==p[31]);
                    assert(kind==ZONE_COM ? p[0]<8U : p[0]>=8U && p[0]<12U);
                    seen |= 1U<<p[0];
                    assert(p==small_lot_art_pixels(21,kind,stage,x,y));
                    assert(p==small_lot_art_pixels(43,kind,stage,x,y));
                }
    assert(seen==4095U); /* Every extracted sprite can appear. */
    for (id=0U; id<1024U; ++id)
        if (id!=0U && (id<21U || id>43U))
            assert(small_lot_art_pixels(id,ZONE_COM,1,0,0)==NULL);
    for (stage=0U; stage<=4U; ++stage) {
        assert(small_lot_art_pixels(0,ZONE_RES,stage,0,0)==NULL);
        assert(small_lot_art_pixels(0,ZONE_NONE,stage,0,0)==NULL);
    }
    assert(small_lot_art_pixels(0,ZONE_COM,0,0,0)==NULL);
    assert(small_lot_art_pixels(0,ZONE_COM,4,0,0)==NULL);
    fixture(SMALL_LOT_ART_BYTES-1U,0);
    assert(!small_lot_art_load("lot_test.dat"));
    assert(small_lot_art_pixels(0,ZONE_COM,1,0,0)==NULL);
    fixture(SMALL_LOT_ART_BYTES+1U,0);
    assert(!small_lot_art_load("lot_test.dat"));
    fixture(SMALL_LOT_ART_BYTES,1);
    assert(!small_lot_art_load("lot_test.dat"));
    assert(remove("lot_test.dat")==0);
    puts("Small lot art: all variants, original-tile isolation, and invalid-pack fallback passed");
    return 0;
}
