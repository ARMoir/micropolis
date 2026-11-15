/* SPDX-License-Identifier: GPL-3.0-or-later
 * Sparse, persistent multi-square building ownership. O(1) tile lookup
 * with a 12KB byte index and <=224 sparse records, no per-frame scans.
 * NEWERM8.DAT contains 797 packed 8x8 tiles from the NewerSource
 * resource library. Never change original source .CTY tile IDs.
 */
#include "newer_lot.h"
#include "city.h"
#include "zoning.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BASE_ID 0x70U
#define BLD_MAGIC_SIZE 8U
#define MAX_RECORD_BYTES (NEWER_LOT_MAX * 5U)
#define BLD_VERSION 1U
#define PACK_TILE_BYTES 32U
/* 3x3 built-in Micropolis styles encoded in the formerly unused
 * low SOURCE-ID range of the existing 5-byte NBL records. No
 * save-format change or additional per-record RAM is needed.
 * 1..4 index four complete original source building families.
 */
#define ORIGINAL_STYLE_FIRST 1U
#define ORIGINAL_STYLE_LAST 4U
#define ZONEBIT 1024U
#define TILEID 1023U
struct newer_record {
    unsigned char x,y,kind,span,id,used;
};
/* Slot number + 1; zero means independently zoned 1x1 lot. */
/* Move the 12KB world index to the FAR heap: fixed DGROUP must stay
 * below 64KB on Open Watcom 8086. Tile lookup remains constant-time.
 */
static unsigned char NEWER_FAR *owner=0;
static struct newer_record records[NEWER_LOT_MAX];
static struct newer_record staged[NEWER_LOT_MAX];
static unsigned int count=0U;
static unsigned char NEWER_FAR *art=0;

static unsigned int offset(unsigned int x,unsigned int y)
{
    return x*CITY_HEIGHT+y;
}
static int ensure_owner(void)
{
    if(owner!=0) return 1;
    owner=(unsigned char NEWER_FAR*)malloc(CITY_TILES);
    if(owner==0) return 0;
    memset(owner,0,CITY_TILES);
    return 1;
}
static int terrain_usable(unsigned int x,unsigned int y,
                          unsigned int kind)
{
    unsigned int id;
    if(x>=CITY_WIDTH || y>=CITY_HEIGHT ||
       zoning_at(x,y)!=kind) return 0;
    id=city_at(x,y)&1023U;
    return id==0U || (id>=21U && id<=43U);
}
static int source_valid(unsigned int kind,unsigned int span,
                        unsigned int id)
{
    if(span==3U && id>=ORIGINAL_STYLE_FIRST &&
       id<=ORIGINAL_STYLE_LAST &&
       kind>=ZONE_RES && kind<=ZONE_IND)return 1;
    if(span==2U) {
        if(kind==ZONE_RES) return id>=0x8cU && id<=0x93U;
        if(kind==ZONE_COM) return id>=0x94U && id<=0x9dU;
        if(kind==ZONE_IND) return id>=0x9eU && id<=0xa5U;
    } else if(span==3U) {
        if(kind==ZONE_RES) return id>=0xaeU && id<=0xb1U;
        if(kind==ZONE_COM) return id>=0xb2U && id<=0xbbU;
        if(kind==ZONE_IND) return id>=0xbcU && id<=0xc1U;
    }
    return 0;
}
/* Offset into the checked-in, complete 797-tile library. */
/* Every built-in option is a COMPLETE 3x3 sprite, not a cut
 * down 1x1 commercial/industrial center tile.
 */
static unsigned int original_base(unsigned int kind,unsigned int id)
{
    if(id<ORIGINAL_STYLE_FIRST || id>ORIGINAL_STYLE_LAST)
        return 65535U;
    if(kind==ZONE_RES)return 261U+9U*(id-1U);
    if(kind==ZONE_COM)return 432U+9U*(id-1U);
    if(kind==ZONE_IND)return 621U+9U*(id-1U);
    return 65535U;
}
static unsigned int original_prefab_base(unsigned int left,
                                         unsigned int top,
                                         unsigned int kind)
{
    unsigned int base,i,j,center;
    unsigned short word;
    if(left+3U>CITY_WIDTH || top+3U>CITY_HEIGHT)
        return 65535U;
    word=city_at(left+1U,top+1U);
    center=word&TILEID;
    if(!(word&ZONEBIT))return 65535U;
    if(kind==ZONE_RES) {
        if(center==244U)base=240U;
        else if(center>=265U && center<=400U &&
                (center-265U)%9U==0U)base=center-4U;
        else return 65535U;
    } else if(kind==ZONE_COM) {
        if(center==427U)base=423U;
        else if(center>=436U && center<=607U &&
                (center-436U)%9U==0U)base=center-4U;
        else return 65535U;
    } else if(kind==ZONE_IND) {
        if(center==616U)base=612U;
        else if(center>=625U && center<=688U &&
                (center-625U)%9U==0U)base=center-4U;
        else return 65535U;
    } else return 65535U;
    for(j=0U;j<3U;++j)
        for(i=0U;i<3U;++i)
            if((city_at(left+i,top+j)&TILEID)!=base+j*3U+i ||
               zoning_at(left+i,top+j)!=kind)
                return 65535U;
    return base;
}
static unsigned int source_start(unsigned int id)
{
    if(id<0x8cU) return id-BASE_ID;
    if(id<0xaeU) return 28U+4U*(id-0x8cU);
    if(id<0xc6U) return 164U+9U*(id-0xaeU);
    return 0U;
}
static unsigned int select_id(unsigned int kind,unsigned int span,
                              unsigned int x,unsigned int y)
{
    unsigned int first,last,choice;
    if(span==2U) {
        first=kind==ZONE_RES?0x8cU:kind==ZONE_COM?0x94U:0x9eU;
        last=kind==ZONE_RES?0x93U:kind==ZONE_COM?0x9dU:0xa5U;
    } else {
        first=kind==ZONE_RES?0xaeU:kind==ZONE_COM?0xb2U:0xbcU;
        last=kind==ZONE_RES?0xb1U:kind==ZONE_COM?0xbbU:0xc1U;
    }
    choice=(x*17U+y*29U+x*y+span*7U)%(last-first+1U);
    return first+choice;
}
static int valid_rect(unsigned int x,unsigned int y,
                      unsigned int kind,unsigned int span,
                      unsigned int existing_slot,unsigned int threshold)
{
    unsigned int i,j,slot;
    if(x+span>CITY_WIDTH || y+span>CITY_HEIGHT) return 0;
    for(j=0U;j<span;++j) for(i=0U;i<span;++i) {
        unsigned int px=x+i,py=y+j;
        if(!terrain_usable(px,py,kind) ||
           zoning_development_at(px,py)<threshold)
            return 0;
        slot=owner[offset(px,py)];
        if(slot!=0U && slot!=existing_slot) return 0;
    }
    return 1;
}
static int find_slot(void)
{
    unsigned int i;
    for(i=0U;i<NEWER_LOT_MAX;++i)
        if(!records[i].used) return (int)i;
    return -1;
}
static void fill_owner(unsigned int slot)
{
    unsigned int i,j,x,y,span;
    struct newer_record *rec=&records[slot];
    x=rec->x;y=rec->y;span=rec->span;
    for(j=0U;j<span;++j)
        for(i=0U;i<span;++i)
            owner[offset(x+i,y+j)]=(unsigned char)(slot+1U);
}
void newer_lot_clear(void)
{
    if(ensure_owner()) memset(owner,0,CITY_TILES);
    memset(records,0,sizeof(records));
    count=0U;
}
unsigned int newer_lot_count(void) {return count;}
int newer_lot_has_art(void){return art!=0;}
int newer_lot_load_art(const char *path)
{
    FILE *f;
    unsigned char NEWER_FAR *candidate;
    if(art!=0) {free(art);art=0;}
    f=fopen(path,"rb");
    if(!f) return 0;
    candidate=(unsigned char NEWER_FAR*)malloc(NEWER_LOT_ART_BYTES);
    if(!candidate) {fclose(f);return 0;}
    if(fread(candidate,1U,NEWER_LOT_ART_BYTES,f)!=NEWER_LOT_ART_BYTES ||
       fgetc(f)!=EOF || ferror(f)) {
        free(candidate);fclose(f);return 0;
    }
    fclose(f);
    art=candidate;
    return 1;
}
void newer_lot_free_art(void)
{
    if(art!=0){free(art);art=0;}
}
int newer_lot_at(unsigned int x,unsigned int y)
{
    if(owner==0 || x>=CITY_WIDTH || y>=CITY_HEIGHT) return 0;
    return owner[offset(x,y)]!=0U;
}
int newer_lot_info(unsigned int x,unsigned int y,
                   unsigned int *left,unsigned int *top,
                   unsigned int *span,unsigned int *kind,
                   unsigned int *source_id)
{
    unsigned int index;
    struct newer_record *r;
    if(owner==0 || x>=CITY_WIDTH || y>=CITY_HEIGHT) return 0;
    index=owner[offset(x,y)];
    if(index==0U || index>NEWER_LOT_MAX)return 0;
    r=&records[index-1U];
    if(!r->used) return 0;
    if(left)*left=r->x;
    if(top)*top=r->y;
    if(span)*span=r->span;
    if(kind)*kind=r->kind;
    if(source_id)*source_id=r->id;
    return 1;
}
const unsigned char NEWER_FAR *newer_lot_pixels(
    unsigned int x,unsigned int y,unsigned int turns)
{
    unsigned int slot,part,span,lx,ly,index,first;
    struct newer_record *rec;
    if(!art || owner==0 || x>=CITY_WIDTH || y>=CITY_HEIGHT) return 0;
    slot=owner[offset(x,y)];
    if(!slot) return 0;
    rec=&records[slot-1U];
    if(rec->id>=ORIGINAL_STYLE_FIRST &&
       rec->id<=ORIGINAL_STYLE_LAST)return 0;
    span=rec->span;
    lx=x-rec->x;ly=y-rec->y;
    /* Display orientation mirrors the existing original-building
     * upright-part selection, without rotating any saved tile data.
     */
    switch(turns&3U) {
    case 1U: part=lx*span+(span-1U-ly);break;
    case 2U: part=(span-1U-ly)*span+(span-1U-lx);break;
    case 3U: part=(span-1U-lx)*span+ly;break;
    default: part=ly*span+lx;break;
    }
    first=source_start(rec->id);
    index=first+part;
    if(index>=797U)return 0;
    return art+index*PACK_TILE_BYTES;
}
/* 50/50 across original detached houses and complete one-tile
 * NewerSource houses, using existing NEWERM8.DAT 0x70..0x7B.
 * Stable by world coordinates so saved cities do not change style
 * on reload; density selects lower/middle/luxury house families.
 */
const unsigned char NEWER_FAR *newer_lot_residential_1x1(
    unsigned int x,unsigned int y,unsigned int stage)
{
    unsigned int index,hash;
    if(!art || stage<1U || stage>3U)return 0;
    /* Mix coordinates so the 50/50 distribution looks organic,
     * not like an artificial chessboard. No per-frame RNG state or
     * extra save bytes: identical world locations render consistently.
     */
    hash=(x*117U+y*193U+x*y*13U)&65535U;
    hash^=hash>>7U;
    hash^=hash>>4U;
    if((hash&1U)==0U)return 0;
    index=(stage-1U)*4U+
          ((hash>>3U)&3U);
    return art+index*PACK_TILE_BYTES;
}
int newer_lot_uses_original(unsigned int x,unsigned int y)
{
    unsigned int slot;
    if(owner==0 || x>=CITY_WIDTH || y>=CITY_HEIGHT)return 0;
    slot=owner[offset(x,y)];
    return slot!=0U && records[slot-1U].used &&
           records[slot-1U].id>=ORIGINAL_STYLE_FIRST &&
           records[slot-1U].id<=ORIGINAL_STYLE_LAST;
}
unsigned int newer_lot_original_tile(unsigned int x,unsigned int y,
                                      unsigned int turns)
{
    unsigned int slot,span,lx,ly,part,base;
    struct newer_record *rec;
    if(!newer_lot_uses_original(x,y))return 65535U;
    slot=owner[offset(x,y)];
    rec=&records[slot-1U];
    span=rec->span;
    lx=x-rec->x;ly=y-rec->y;
    switch(turns&3U) {
    case 1U:part=lx*span+(span-1U-ly);break;
    case 2U:part=(span-1U-ly)*span+(span-1U-lx);break;
    case 3U:part=(span-1U-lx)*span+ly;break;
    default:part=ly*span+lx;break;
    }
    base=original_base(rec->kind,rec->id);
    return base==65535U?65535U:base+part;
}
/* Preserve an intact imported classic 3x3 prefab in .CTY, but
 * register a NEWER facade on its same footprint when its real
 * growth event selects NewerSource. The underlying original can
 * still be recovered from the save with no destructive conversion.
 */
int newer_lot_retrofit_original(unsigned int cx,unsigned int cy,
                                unsigned int kind)
{
    unsigned int left,top,i,j,index;
    int available;
    struct newer_record *rec;
    if(!art || cx==0U || cy==0U ||
       cx+1U>=CITY_WIDTH || cy+1U>=CITY_HEIGHT ||
       !ensure_owner())return 0;
    left=cx-1U;top=cy-1U;
    if(original_prefab_base(left,top,kind)==65535U)return 0;
    for(j=0U;j<3U;++j)
        for(i=0U;i<3U;++i)
            if(owner[offset(left+i,top+j)]!=0U)return 0;
    available=find_slot();
    if(available<0)return 0;
    index=(unsigned int)available;
    rec=&records[index];
    rec->x=(unsigned char)left;
    rec->y=(unsigned char)top;
    rec->kind=(unsigned char)kind;
    rec->span=3U;
    rec->id=(unsigned char)select_id(kind,3U,left,top);
    rec->used=1U;
    fill_owner(index);
    ++count;
    return 1;
}
/* Simulated new 2x2->3x3 growth uses one independent 50/50
 * source-style decision. Standalone 2x2 has no original COMPLETE
 * source equivalent, so it always uses original unmodified NEWER art.
 * The existing entry point remains for older callers/tests.
 */
unsigned int newer_lot_try_grow_with_style(unsigned int x,unsigned int y,
                                           unsigned int kind,
                                           unsigned int choose_original,
                                           unsigned int *left,
                                           unsigned int *top)
{
    unsigned int span,ax=0U,ay=0U,slot;
    span=newer_lot_try_grow(x,y,kind,&ax,&ay);
    if(span==3U && choose_original && owner!=0) {
        slot=owner[offset(ax,ay)];
        if(slot!=0U) {
            records[slot-1U].id=(unsigned char)
                (1U+((ax*17U+ay*29U+kind)&3U));
        }
    }
    if(span!=0U) {
        if(left)*left=ax;
        if(top)*top=ay;
    }
    return span;
}
unsigned int newer_lot_try_grow(unsigned int x,unsigned int y,
                                 unsigned int kind,
                                 unsigned int *left,unsigned int *top)
{
    unsigned int slot,i,j,cx,cy,ax,ay;
    unsigned int index;
    int available;
    struct newer_record *rec;
    if(x>=CITY_WIDTH || y>=CITY_HEIGHT ||
       kind<ZONE_RES || kind>ZONE_IND || !ensure_owner()) return 0U;
    slot=owner[offset(x,y)];
    if(slot!=0U) {
        rec=&records[slot-1U];
        if(!rec->used || rec->span!=2U || rec->kind!=kind ||
           rec->x!=x || rec->y!=y) return 0U;
        /* A single 2x2 apartment/office/factory may absorb its
         * adjacent 1x1 plots when a 3x3 square becomes mature.
         * All four possible placements around the existing 2x2
         * are considered; no other multi-square building is erased.
         */
        ax=x;ay=y;
        for(j=0U;j<2U;++j)for(i=0U;i<2U;++i) {
            unsigned int row,col;
            if(ax<i || ay<j)continue;
            cx=ax-i;cy=ay-j;
            if(!valid_rect(cx,cy,kind,3U,slot,2U))continue;
            /* Free old owner cells, grow in place using SAME slot. */
            for(row=0U;row<2U;++row)
                for(col=0U;col<2U;++col)
                    owner[offset(ax+col,ay+row)]=0U;
            rec->x=(unsigned char)cx;
            rec->y=(unsigned char)cy;
            rec->span=3U;
            rec->id=(unsigned char)select_id(kind,3U,cx,cy);
            fill_owner(slot-1U);
            if(left)*left=cx;
            if(top)*top=cy;
            return 3U;
        }
        return 0U;
    }
    available=find_slot();
    if(available<0 || !valid_rect(x,y,kind,2U,0U,3U))
        return 0U;
    index=(unsigned int)available;
    rec=&records[index];
    rec->x=(unsigned char)x;rec->y=(unsigned char)y;
    rec->kind=(unsigned char)kind;rec->span=2U;
    rec->id=(unsigned char)select_id(kind,2U,x,y);
    rec->used=1U;
    fill_owner(index);
    ++count;
    if(left)*left=x;
    if(top)*top=y;
    return 2U;
}
unsigned int newer_lot_demolish(unsigned int x,unsigned int y,
                                unsigned int *left,unsigned int *top)
{
    unsigned int slot,i,j,span,ax,ay;
    struct newer_record *rec;
    if(owner==0 || x>=CITY_WIDTH || y>=CITY_HEIGHT)return 0U;
    slot=owner[offset(x,y)];
    if(!slot)return 0U;
    rec=&records[slot-1U];
    ax=rec->x;ay=rec->y;span=rec->span;
    /* If this was a NewerSource facelift of an imported original
     * 3x3, also demolish the intact hidden original prefab.
     * Otherwise old source art would pop back through the cleared
     * overlay after bulldozing a single piece.
     */
    {
        int classic=(span==3U &&
            original_prefab_base(ax,ay,rec->kind)!=65535U);
        for(j=0U;j<span;++j)for(i=0U;i<span;++i) {
            if(classic)city_set(ax+i,ay+j,0U);
            owner[offset(ax+i,ay+j)]=0U;
            (void)zoning_set_development(ax+i,ay+j,0U);
        }
    }
    memset(rec,0,sizeof(*rec));
    --count;
    if(left)*left=ax;
    if(top)*top=ay;
    return span;
}
static unsigned int hash_byte(unsigned int hash,unsigned int value)
{
    return (((hash<<5U)|(hash>>11U))^value)&65535U;
}
static unsigned int hash_record(unsigned int hash,const struct newer_record *r)
{
    hash=hash_byte(hash,r->x);
    hash=hash_byte(hash,r->y);
    hash=hash_byte(hash,r->kind);
    hash=hash_byte(hash,r->span);
    return hash_byte(hash,r->id);
}
int newer_lot_save(const char *path)
{
    static const unsigned char magic[8]={'N','W','L','T',BLD_VERSION,
                                         CITY_WIDTH,CITY_HEIGHT,0};
    FILE *f;
    unsigned int i,hash=0x4961U;
    int okay;
    f=fopen(path,"wb");
    if(!f)return 0;
    okay=fwrite(magic,1U,8U,f)==8U;
    okay=okay&&fputc(count&255U,f)!=EOF;
    okay=okay&&fputc(count>>8U,f)!=EOF;
    for(i=0U;i<NEWER_LOT_MAX && okay;++i)
        if(records[i].used) {
            struct newer_record *r=&records[i];
            okay=fputc(r->x,f)!=EOF&&fputc(r->y,f)!=EOF&&
                 fputc(r->kind,f)!=EOF&&fputc(r->span,f)!=EOF&&
                 fputc(r->id,f)!=EOF;
            hash=hash_record(hash,r);
        }
    okay=okay&&fputc(hash&255U,f)!=EOF;
    okay=okay&&fputc(hash>>8U,f)!=EOF;
    if(fclose(f)!=0)okay=0;
    return okay;
}
int newer_lot_load(const char *path)
{
    static const unsigned char magic[8]={'N','W','L','T',BLD_VERSION,
                                         CITY_WIDTH,CITY_HEIGHT,0};
    FILE *f;
    unsigned int i,j,k,count_in,hash=0x4961U,expected;
    int c;
    long length;
    f=fopen(path,"rb");
    if(!f)return 0;
    if(fseek(f,0L,SEEK_END)!=0){fclose(f);return 0;}
    length=ftell(f);
    if(length<12L || length>12L+(long)MAX_RECORD_BYTES ||
       fseek(f,0L,SEEK_SET)!=0){fclose(f);return 0;}
    for(i=0U;i<8U;++i)if(fgetc(f)!=magic[i]) {
        fclose(f);return 0;
    }
    c=fgetc(f);
    if(c==EOF){fclose(f);return 0;}
    count_in=(unsigned int)c;
    c=fgetc(f);
    if(c==EOF){fclose(f);return 0;}
    count_in|=(unsigned int)c<<8U;
    if(count_in>NEWER_LOT_MAX ||
       length!=12L+(long)count_in*5L){fclose(f);return 0;}
    for(i=0U;i<count_in;++i) {
        struct newer_record *rec=&staged[i];
        c=fgetc(f);if(c==EOF){fclose(f);return 0;}
        rec->x=(unsigned char)c;
        c=fgetc(f);if(c==EOF){fclose(f);return 0;}
        rec->y=(unsigned char)c;
        c=fgetc(f);if(c==EOF){fclose(f);return 0;}
        rec->kind=(unsigned char)c;
        c=fgetc(f);if(c==EOF){fclose(f);return 0;}
        rec->span=(unsigned char)c;
        c=fgetc(f);if(c==EOF){fclose(f);return 0;}
        rec->id=(unsigned char)c;
        rec->used=1U;
        hash=hash_record(hash,rec);
        if(!source_valid(rec->kind,rec->span,rec->id) ||
           rec->x+rec->span>CITY_WIDTH ||
           rec->y+rec->span>CITY_HEIGHT) {
            fclose(f);return 0;
        }
        for(j=0U;j<rec->span;++j)
            for(k=0U;k<rec->span;++k) {
                unsigned int xx=rec->x+k,yy=rec->y+j;
                unsigned int old;
                if((!terrain_usable(xx,yy,rec->kind) ||
                     zoning_development_at(xx,yy)<2U) &&
                   !(rec->span==3U &&
                     original_prefab_base(rec->x,rec->y,
                                          rec->kind)!=65535U)) {
                    fclose(f);return 0;
                }
                for(old=0U;old<i;++old) {
                    struct newer_record *prev=&staged[old];
                    if(xx>=prev->x&&xx<prev->x+prev->span&&
                       yy>=prev->y&&yy<prev->y+prev->span){
                        fclose(f);return 0;
                    }
                }
            }
    }
    c=fgetc(f);if(c==EOF){fclose(f);return 0;}
    expected=(unsigned int)c;
    c=fgetc(f);if(c==EOF){fclose(f);return 0;}
    expected|=(unsigned int)c<<8U;
    if(fclose(f)!=0 || expected!=hash || !ensure_owner())return 0;
    newer_lot_clear();
    for(i=0U;i<count_in;++i) {
        records[i]=staged[i];
        fill_owner(i);
    }
    count=count_in;
    return 1;
}
