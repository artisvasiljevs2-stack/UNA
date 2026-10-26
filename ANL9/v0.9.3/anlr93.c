#include "anl93.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t anl93_crc32(const void *data, size_t n) {
    const uint8_t *p = data; uint32_t c = ~0u;
    for (size_t i=0;i<n;++i) { c^=p[i]; for(int b=0;b<8;++b)c=(c&1u)?(c>>1)^0xedb88320u:c>>1; }
    return ~c;
}
static int append(uint8_t *out,size_t *used,size_t cap,const void *p,size_t n) {
    if (*used>cap || n>cap-*used) return 0;
    if (n) memmove(out+*used,p,n);
    *used+=n;
    return 1;
}
static int write_all(FILE *f,const void *p,size_t n) { return n==0 || fwrite(p,1,n,f)==n; }
int main(int argc,char **argv) {
    if(argc!=3){fprintf(stderr,"ANLR 0.9.3\nUsage: %s <archive> <output>\n",argv[0]);return 1;}
    FILE *f=fopen(argv[1],"rb"); if(!f){perror("archive");return 1;}
    ANL93Header h; uint8_t *out=NULL; FILE *g=NULL; int ok=0;
    if(fread(&h,sizeof h,1,f)!=1) goto done;
    uint32_t saved=h.header_crc32; h.header_crc32=0;
    if(memcmp(h.magic,ANL93_MAGIC,4)||h.version!=ANL93_VERSION||h.block_size!=ANL93_BLOCK_SIZE||h.table_count!=0||h.record_count!=h.block_count||anl93_crc32(&h,sizeof h)!=saved) goto done;
    h.header_crc32=saved;
    uint64_t expected_blocks=h.original_size/h.block_size+(h.original_size%h.block_size!=0u);
    if(h.original_size>SIZE_MAX || expected_blocks!=h.block_count) goto done;
    size_t cap=(size_t)h.original_size,used=0; out=malloc(cap?cap:1); if(!out) goto done;
    for(uint32_t bi=0;bi<h.block_count;++bi){
        ANL93Record r; if(fread(&r,sizeof r,1,f)!=1||r.block_index!=bi||r.header_size!=sizeof r||r.flags!=0||r.payload_size>h.block_size)goto done;
        size_t expected=cap-used; if(expected>h.block_size)expected=h.block_size;
        uint8_t *p=r.payload_size?malloc(r.payload_size):NULL;
        if(r.payload_size&&(!p||fread(p,1,r.payload_size,f)!=r.payload_size||anl93_crc32(p,r.payload_size)!=r.crc32)){free(p);goto done;}
        size_t start=used;
        if(r.type==ANL93_RAW){if(r.payload_size!=expected||!append(out,&used,cap,p,r.payload_size)){free(p);goto done;}}
        else if(r.type==ANL93_BLOCK_REF){uint32_t src;if(r.payload_size!=4||expected==0){free(p);goto done;}memcpy(&src,p,4);size_t off=(size_t)src*h.block_size;if(src>=bi||off>used||expected>used-off||!append(out,&used,cap,out+off,expected)){free(p);goto done;}}
        else if(r.type==ANL93_MINIBLOCK){
            if(r.payload_size<sizeof(ANL93MiniHeader)){free(p);goto done;} ANL93MiniHeader m;memcpy(&m,p,sizeof m);size_t pos=sizeof m;
            if(m.output_size!=expected||m.operation_count>(r.payload_size-sizeof m)/sizeof(ANL93MiniOp)){free(p);goto done;}
            for(uint32_t j=0;j<m.operation_count;++j){if(pos>r.payload_size||sizeof(ANL93MiniOp)>r.payload_size-pos){free(p);goto done;}ANL93MiniOp x;memcpy(&x,p+pos,sizeof x);pos+=sizeof x;if(x.reserved[0]||x.reserved[1]||x.reserved[2]||x.length==0){free(p);goto done;}
                if(x.opcode==ANL93_MB_DATA){if(x.length>r.payload_size-pos||!append(out,&used,cap,p+pos,x.length)){free(p);goto done;}pos+=x.length;}
                else if(x.opcode==ANL93_MB_REF){if(x.argument0>used||x.length>used-x.argument0||!append(out,&used,cap,out+x.argument0,x.length)){free(p);goto done;}}
                else if(x.opcode==ANL93_MB_REPEAT){if(x.argument0>255u){free(p);goto done;}uint8_t v=(uint8_t)x.argument0;for(uint32_t k=0;k<x.length;++k)if(!append(out,&used,cap,&v,1)){free(p);goto done;}}
                else {free(p);goto done;}
                if(used-start>expected){free(p);goto done;}
            }
            if(pos!=r.payload_size||used-start!=expected){free(p);goto done;}
        } else {free(p);goto done;}
        free(p);
    }
    if(used!=cap||anl93_crc32(out,used)!=h.final_crc32||fgetc(f)!=EOF||ferror(f))goto done;
    g=fopen(argv[2],"wb");if(!g||!write_all(g,out,used)||fflush(g)!=0||fclose(g)!=0){g=NULL;goto done;}g=NULL;
    printf("# ANLR 0.9.3\nOutput size: %zu\nCRC32: %08" PRIX32 "\nDecompression successful.\n",used,h.final_crc32);ok=1;
done:
    if(g) fclose(g);
    if(!ok) fprintf(stderr,"ERROR: corrupt, truncated, or unsafe ANL9 archive.\n");
    free(out);
    fclose(f);
    return ok?0:1;
}
