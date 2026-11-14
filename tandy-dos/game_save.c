/* SPDX-License-Identifier: GPL-3.0-or-later
 * Transaction-like 3-file save on FAT/XT-IDE CF using DOS 8.3 names.
 * Staging protects old saves from media/full-disk write failures.
 * Backups preserve the previous successful version of each sidecar.
 */
#include "game_save.h"
#include "city.h"
#include "zoning.h"
#include "newer_lot.h"
#include <stdio.h>
#include <string.h>

#define GAME_SAVE_MAX 9999U
#define GAME_SAVE_FILES 3U

static void name(unsigned int slot,unsigned int which,
                 const char *ext,char *output)
{
    (void)which;
    sprintf(output,"CITY%04u.%s",slot,ext);
}
static int file_exists(const char *path)
{
    FILE *f=fopen(path,"rb");
    if(f==NULL)return 0;
    fclose(f);
    return 1;
}
static void names_for(unsigned int slot,char names[3][13],
                      char tmp[3][13],char bak[3][13])
{
    name(slot,0U,"CTY",names[0]);
    name(slot,1U,"ZON",names[1]);
    name(slot,2U,"NBL",names[2]);
    name(slot,0U,"TCT",tmp[0]);
    name(slot,1U,"TZN",tmp[1]);
    name(slot,2U,"TNB",tmp[2]);
    name(slot,0U,"BCT",bak[0]);
    name(slot,1U,"BZN",bak[1]);
    name(slot,2U,"BNB",bak[2]);
}
static unsigned int available_slot(void)
{
    unsigned int slot,i;
    char dst[3][13],tmp[3][13],bak[3][13];
    for(slot=1U;slot<=GAME_SAVE_MAX;++slot) {
        names_for(slot,dst,tmp,bak);
        for(i=0U;i<GAME_SAVE_FILES;++i)
            if(file_exists(dst[i]) || file_exists(tmp[i]) ||
               file_exists(bak[i]))break;
        if(i==GAME_SAVE_FILES)return slot;
    }
    return 0U;
}
static void cleanup_temps(char tmp[3][13])
{
    unsigned int i;
    for(i=0U;i<GAME_SAVE_FILES;++i) (void)remove(tmp[i]);
}
static int save_last_index(unsigned int slot)
{
    FILE *f;
    int okay;
    unsigned int stamp=slot^0xa54cU;
    /* The marker is deliberately tiny, so an incomplete filesystem
     * write cannot masquerade as a valid last-save slot.
     */
    (void)remove("LASTCITY.TMP");
    f=fopen("LASTCITY.TMP","wb");
    if(!f)return 0;
    okay=fputc('L',f)!=EOF && fputc('C',f)!=EOF &&
         fputc('0',f)!=EOF && fputc('1',f)!=EOF &&
         fputc(slot&255U,f)!=EOF &&
         fputc((slot>>8U)&255U,f)!=EOF &&
         fputc(stamp&255U,f)!=EOF &&
         fputc((stamp>>8U)&255U,f)!=EOF;
    if(fclose(f)!=0)okay=0;
    if(!okay){(void)remove("LASTCITY.TMP");return 0;}
    /* Losing this optional index cannot corrupt the CTY/ZON/NBL.
     * On old DOS, rename cannot overwrite: remove the old marker.
     */
    (void)remove("LASTCITY.CFG");
    if(rename("LASTCITY.TMP","LASTCITY.CFG")!=0)return 0;
    return 1;
}
unsigned int game_save_write(unsigned int *slot)
{
    unsigned int chosen,i,j,old[3]={0U,0U,0U};
    unsigned int moved[3]={0U,0U,0U};
    unsigned int code=GAME_SAVE_OK;
    char dst[3][13],tmp[3][13],bak[3][13];
    if(!slot)return GAME_SAVE_NO_SLOT;
    chosen=*slot;
    if(chosen==0U)chosen=available_slot();
    if(chosen==0U || chosen>GAME_SAVE_MAX)
        return GAME_SAVE_NO_SLOT;
    names_for(chosen,dst,tmp,bak);
    cleanup_temps(tmp);
    if(!city_save_as(tmp[0])) {
        code=GAME_SAVE_CTY_ERROR;goto fail;
    }
    if(!zoning_save(tmp[1])) {
        code=GAME_SAVE_ZON_ERROR;goto fail;
    }
    if(!newer_lot_save(tmp[2])) {
        code=GAME_SAVE_NBL_ERROR;goto fail;
    }
    /* Staging finished; move old versions to backups before replacing
     * ANY of the live city components. Roll back if a rename fails.
     */
    for(i=0U;i<GAME_SAVE_FILES;++i) {
        old[i]=(unsigned int)file_exists(dst[i]);
        if(!old[i])continue;
        (void)remove(bak[i]);
        if(rename(dst[i],bak[i])!=0) {
            code=GAME_SAVE_COMMIT_ERROR;
            goto rollback;
        }
        moved[i]=1U;
    }
    for(i=0U;i<GAME_SAVE_FILES;++i) {
        if(rename(tmp[i],dst[i])!=0) {
            code=GAME_SAVE_COMMIT_ERROR;
            /* Remove only newly installed files, then recover all
             * prior versions from .BCT/.BZN/.BNB backup files.
             */
            for(j=0U;j<i;++j)(void)remove(dst[j]);
            goto rollback;
        }
    }
    *slot=chosen;
    if(!save_last_index(chosen))
        return GAME_SAVE_LAST_ERROR;
    return GAME_SAVE_OK;

rollback:
    for(i=0U;i<GAME_SAVE_FILES;++i)
        if(moved[i]) (void)rename(bak[i],dst[i]);
fail:
    cleanup_temps(tmp);
    return code;
}
static unsigned int dos_digit(char c)
{
    if(c>='0' && c<='9')return (unsigned int)(c-'0');
    return 10U;
}
static char uppercase(char c)
{
    if(c>='a' && c<='z')return (char)(c-'a'+'A');
    return c;
}
int game_save_parse_slot(const char *filename,unsigned int *slot)
{
    unsigned int i,result=0U;
    static const char *prefix="CITY";
    static const char *extension=".CTY";
    if(!filename || !slot || strlen(filename)!=12U)return 0;
    for(i=0U;i<4U;++i)
        if(uppercase(filename[i])!=prefix[i])return 0;
    for(i=4U;i<8U;++i) {
        unsigned int digit=dos_digit(filename[i]);
        if(digit>9U)return 0;
        result=result*10U+digit;
    }
    for(i=0U;i<4U;++i)
        if(uppercase(filename[i+8U])!=extension[i])return 0;
    if(result==0U)return 0;
    *slot=result;
    return 1;
}
int game_save_last_slot(unsigned int *slot)
{
    FILE *f;
    unsigned int v,stamp;
    int a,b,c,d;
    char dst[3][13],tmp[3][13],bak[3][13];
    if(!slot)return 0;
    f=fopen("LASTCITY.CFG","rb");
    if(!f)return 0;
    if(fgetc(f)!='L'||fgetc(f)!='C'||
       fgetc(f)!='0'||fgetc(f)!='1') {
        fclose(f);return 0;
    }
    a=fgetc(f);b=fgetc(f);c=fgetc(f);d=fgetc(f);
    if(a==EOF||b==EOF||c==EOF||d==EOF||
       fgetc(f)!=EOF) {
        fclose(f);return 0;
    }
    if(fclose(f)!=0)return 0;
    v=(unsigned int)a|((unsigned int)b<<8U);
    stamp=(unsigned int)c|((unsigned int)d<<8U);
    if(v==0U||v>GAME_SAVE_MAX||(v^0xa54cU)!=stamp)return 0;
    names_for(v,dst,tmp,bak);
    if(!file_exists(dst[0]) || !file_exists(dst[1]) ||
       !file_exists(dst[2]))return 0;
    *slot=v;
    return 1;
}
int game_save_load_last(unsigned int *slot)
{
    unsigned int current;
    char dst[3][13],tmp[3][13],bak[3][13];
    if(!slot || !game_save_last_slot(&current))return 0;
    names_for(current,dst,tmp,bak);
    if(!city_load(dst[0]))return 0;
    zoning_seed_from_city();
    if(!zoning_load(dst[1]))return 0;
    newer_lot_clear();
    if(!newer_lot_load(dst[2]))return 0;
    *slot=current;
    return 1;
}
