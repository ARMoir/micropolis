/* SPDX-License-Identifier: GPL-3.0-or-later
 * Display-only ground policy. No changes to city, zoning or DAT artwork.
 */
#include "ground.h"
#include "city.h"
#include "zoning.h"
#include "newer_lot.h"
unsigned char ground_tint[2][256];
void ground_init(void)
{
    unsigned int p,b,hi,lo;
    for(p=0U;p<2U;++p)for(b=0U;b<256U;++b) {
        hi=b>>4;lo=b&15U;
        if(hi==2U)hi=6U;
        if(lo==2U)lo=6U;
        ground_tint[p][b]=(unsigned char)((hi<<4)|lo);
    }
}
static int forest_at(unsigned int x,unsigned int y)
{
    unsigned int id=city_at(x,y)&1023U;
    return id>=21U && id<=43U &&
           zoning_development_at(x,y)==0U && !newer_lot_at(x,y);
}
int ground_keep_green(unsigned int x,unsigned int y)
{
    unsigned int xx,yy,x0,x1,y0,y1;
    if(x>=CITY_WIDTH || y>=CITY_HEIGHT)return 0;
    x0=x?x-1U:0U;y0=y?y-1U:0U;
    x1=x+1U<CITY_WIDTH?x+1U:x;
    y1=y+1U<CITY_HEIGHT?y+1U:y;
    for(yy=y0;yy<=y1;++yy)for(xx=x0;xx<=x1;++xx)
        if(forest_at(xx,yy))return 1;
    return 0;
}
int ground_is_clear(unsigned int x,unsigned int y)
{
    return x<CITY_WIDTH && y<CITY_HEIGHT &&
           (city_at(x,y)&1023U)==0U &&
           zoning_development_at(x,y)==0U && !newer_lot_at(x,y);
}
