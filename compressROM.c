#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void error(int errorcode);
uint16_t simplelz(uint8_t* fload,uint8_t* store,uint16_t filesize);
void printOut(FILE *fp,uint8_t *buffer,uint16_t filesize,char *name);

// convert binary ROM file to compressed const uint8_t array, pads 8kB ROMs with zeros if needed
int main(int argc, char* argv[]) {
	if (argc < 4) {
        fprintf(stdout,"Usage compressrom headername infile outfile\n");   
        exit(0);
    }
    // open files
    FILE *fp_in,*fp_out;
    if ((fp_in=fopen(argv[2],"rb"))==NULL) error(1);
    fseek(fp_in, 0, SEEK_END); // jump to the end of the file to get the length
	uint16_t filesize = ftell(fp_in); // get the file size
    rewind(fp_in);
    if(filesize>16384) error(2); // too large to be a ROM so error
    //
    uint8_t *buffer,*readin,*comp;
    if((readin=malloc(16384))==NULL) error(3); // cannot allocate memory
    for(uint i=0;i<16384;i++) readin[i]=0x00; // clear readin buffer, has the affect of padding 8kB ROMs
    if(fread(readin,sizeof(uint8_t),filesize,fp_in)<filesize) error(4); // cannot read enough bytes in
    fclose(fp_in);
    //
    if((comp=malloc(16384+32))==NULL) error(3); // cannot allocate memory for compression
    uint16_t compsize=simplelz(readin,comp,16384);
    free(readin);    
    //
    if ((fp_out=fopen(argv[3],"wb"))==NULL) error(1); 
    printOut(fp_out,comp,compsize,argv[1]);
    free(comp);
    fclose(fp_out);
    //
    return 0;
}

void printOut(FILE *fp,uint8_t *buffer,uint16_t filesize,char *name) {
    uint i,j;
    fprintf(fp,"    const uint8_t %s[]={ ",name);
    for(i=0;i<filesize;i++) {
        if((i%16)==0&&i!=0) {
            fprintf(fp,"\n");
            for(j=0;j<23+strlen(name);j++) fprintf(fp," ");   
        }
        fprintf(fp,"0x%02x",buffer[i]);
        if(i<filesize-1) {
            fprintf(fp,",");
        }
    }
    fprintf(fp," };\n // %dbytes",filesize);
}

//
// very simple lz with 256byte backward look
// 
// x=128+ then copy sequence from x-offset from next byte offset 
// x=0-127 then copy literal x+1 times
// minimum sequence size 2
uint16_t simplelz(uint8_t* fload,uint8_t* store,uint16_t filesize)
{
	uint16_t i;
	uint8_t * store_p, * store_c;
	uint8_t litsize = 1;
	uint16_t repsize, offset, repmax, offmax;
	store_c = store;
	store_p = store_c + 1;
	//
	i = 0;
	*store_p++ = fload[i++];
	do {
		// scan for sequence
		repmax = 2;
		if (i > 255) offset = i - 256; else offset = 0;
		do {
			repsize = 0;
			while (fload[offset + repsize] == fload[i + repsize] && i + repsize < filesize && repsize < 129) {
				repsize++;
			}
			if (repsize > repmax) {
				repmax = repsize;
				offmax = i - offset;
			}
			offset++;
		} while (offset < i && repmax < 129);
		if (repmax > 2) {
			if (litsize > 0) {
				*store_c = litsize - 1;
				store_c = store_p++;
				litsize = 0;
			}
			*store_p++ = offmax - 1; //1-256 -> 0-255
			*store_c = repmax + 126;
			store_c = store_p++;
			i += repmax;
		}
		else {
			litsize++;
			*store_p++ = fload[i++];
			if (litsize > 127) {
				*store_c = litsize - 1;
				store_c = store_p++;
				litsize = 0;
			}
		}
	} while (i < filesize);
	if (litsize > 0) {
		*store_c = litsize - 1;
		store_c = store_p++;
	}
	*store_c = 128;	// end marker
	return store_p - store;
}

//
// E01 - cannot open input/output file
// E02 - incorrect ROM file
// E03 - cannot allocate memory
// E04 - problem reading ROM file
void error(int errorcode) {
	fprintf(stdout, "[E%02d]\n", errorcode);
	exit(errorcode);
}