#include <stdio.h>
#include <string.h>
#include <cmath>


#define SCRIPTFILE      "script.txt"

#define KEYWORD_MAX    16
#define LABEL_MAX      16

static int script_nbr_seq;
static int script_cur_seq;
static char script_cur_name[LABEL_MAX];
static int script_cur_timeout;

static char line[512];
static char textualdata[8][256];
static int textualdata_index = 0; 

static int initScript(void) {
    script_nbr_seq = 0;
    char keyword[KEYWORD_MAX];
    char c;
    int i;     
    FILE * fd = fopen(SCRIPTFILE, "r");
    if (fd)  {
        printf("file opened\n"); 
        i = 0;
        while ((c = fgetc(fd) ) != EOF) {
            //printf("0x%02x %c\n",c,c);
            if (c == '\n') { 
                line[i] = 0;
                if (strlen(&line[0])) {
                    //printf("%s\n",&line[0]);
                    sscanf(&line[0], "%s", &keyword[0]); 
                    int kewordlen = strlen(&keyword[0]);
                    if (!strncmp("begin",keyword,kewordlen)) {
                        script_nbr_seq++;
                    }
                }
                i = 0;  
            }
            else { 
                line[i++] = c;
            }
        }  
        fclose(fd);
    }

    return (script_nbr_seq);
}

static void loadScriptSeq(int seq) {
    FILE * fd;
    int cur_seq = 0;       
    char keyword[KEYWORD_MAX];
    char c;
    int i; 
    fd = fopen(SCRIPTFILE, "r");
    if (fd) {
        printf("file opened\n"); 
        i = 0;
        while ((c = fgetc(fd) ) != EOF) {
            //printf("0x%02x %c\n",c,c);
            if (c == '\n') { 
                line[i] = 0;
                if (strlen(&line[0])) {
                    //printf("%s\n",&line[0]);
                    sscanf(&line[0], "%s", &keyword[0]); 
                    int kewordlen = strlen(&keyword[0]);
                    //printf("%s %d\n",&keyword[0], kewordlen);
                    if (!strncmp("//",keyword,kewordlen)) {
                        printf("comment\n");
                    }
                    else if (!strncmp("begin",keyword,kewordlen)) {
                        if  (cur_seq == seq) {
                            sscanf(&line[0], "%s %s", &keyword[0], &script_cur_name[0]);                      
                            printf("begin %s\n", &script_cur_name[0]);
                            textualdata_index = 0;
                        }
                    }
                    else if (!strncmp("end",keyword,kewordlen)) { 
 //                       sscanf(&line[0], "%s %d", &keyword[0], &delay);                      
 //                       printf("end %d\n", delay);
                       if (cur_seq == seq ) {
                            sscanf(&line[0], "%s %d", &keyword[0], &script_cur_timeout);                      
                            printf("end %d\n", script_cur_timeout);
                            break;
                        }
                        else {
                            cur_seq++;
                        }     

                    }
                    else if ( (!strncmp("text",keyword,kewordlen)) && (cur_seq == seq) ) {
                        int fnth;
                        int yoff;
                        int speed;
                        int opts;
                        int r,g,b;
                        int amp;
                        int nbparam = sscanf(&line[0], "%s %d,%d,%d, 0x%02x,%d,%d,%d,%d,%[^\n]", &keyword[0], &fnth, &yoff,&speed, &opts, &r,&g,&b, &amp, &textualdata[textualdata_index][0]);                      
                        //text 8,5, 3, 0x22, 255,255,255, hello les miloutis 
                        printf("text %d,%d,%d, 0x%02x, (%d,%d,%d), %d, %s\n",fnth,yoff,speed, opts, r,g,b, amp, &textualdata[textualdata_index][1]);
                        textualdata_index++;
                    }
                    else if ( (!strncmp("gif",keyword,kewordlen)) && (cur_seq == seq) ) { 
                        int yoff;
                        int speed;
                        int opts;
                        int amp;
                        int nbparam = sscanf(&line[0], "%s %d,%d, 0x%02x,%d,%[^\n]", &keyword[0], &yoff, &speed, &opts, &amp, &textualdata[textualdata_index][0]);
                        //gif -12, 2, 0x22, perno.gif   
                        printf("gif %d,%d, 0x%02x, %d, %s\n", yoff, speed, opts, amp, &textualdata[textualdata_index][1]);
                        textualdata_index++;
                    }
                    else if ( (!strncmp("raster",keyword,kewordlen)) && (cur_seq == seq) ) { 
                        int yoff;
                        int height;
                        int speed;
                        int opts;
                        int amp;
                        int nbparam = sscanf(&line[0], "%s %d,%d, %d, 0x%02x,%d,%[^\n]", &keyword[0], &height, &yoff, &speed, &opts, &amp, &textualdata[textualdata_index][0]);
                        //raster 4,0, 3, 0x22, R   
                        printf("raster %d,%d,%d 0x%02x, %d, %s\n", height, yoff, speed, opts, amp, &textualdata[textualdata_index][1]);
                        textualdata_index++;
                    }
                }
                i = 0;  
            }
            else { 
                line[i++] = c;
            }
        }  
        fclose(fd);

    }
}



/************************************************
 * MAIN 
 ************************************************/
int main(void) {
    printf("start\n");   
    initScript();
    printf("nb seq: %d \n", script_nbr_seq);   
    loadScriptSeq(1);

}






