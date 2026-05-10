#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "lodepng.h"

unsigned char* load_png(char* filename, unsigned int* width, unsigned int* height){
    unsigned char* image = NULL;
    int error = lodepng_decode32_file(&image, width, height, filename);
    if (error != 0)
        printf("error %u: %s\n", error, lodepng_error_text(error));
    return image;
}

void write_png(char* filename, unsigned char* image, unsigned width, unsigned height){
    unsigned char* png;
    long long unsigned int pngsize;
    int error = lodepng_encode32(&png, &pngsize, image, width, height);
    if (error == 0)
        lodepng_save_file(png, pngsize, filename);
    else
        printf("error %u: %s\n", error, lodepng_error_text(error));
    free(png);
}

void contrast(unsigned char *col, int bw_size){
    int i;
    for(i=0; i < bw_size; i++){
        if (col[i] < 105) 
            col[i] = 0;
        else 
            col[i] = 255;
    }
    return;
}

void Gauss_blur(unsigned char *col, unsigned char *blr_pic, int width, int height){
    int i, j;
    for(i=1; i < height-1; i++){
        for(j=1; j < width-1; j++){
            float val = 0.084f*col[width*i+j] + 0.084f*col[width*(i+1)+j] + 0.084f*col[width*(i-1)+j];
            val += 0.084f*col[width*i+(j+1)] + 0.084f*col[width*i+(j-1)];
            val += 0.063f*col[width*(i+1)+(j+1)] + 0.063f*col[width*(i+1)+(j-1)];
            val += 0.063f*col[width*(i-1)+(j+1)] + 0.063f*col[width*(i-1)+(j-1)];
            blr_pic[width*i+j] = val;
        }
    }
    return;
}

void color(unsigned char *blr_pic, unsigned char *res, int size){
    int i;
    for(i=1;i<size;i++){
        res[i*4] = 40 + blr_pic[i] + 0.35f*blr_pic[i-1];
        res[i*4+1] = 65 + blr_pic[i];
        res[i*4+2] = 170 + blr_pic[i];
        res[i*4+3] = 255;
    }
    return;
}

void to_bw(unsigned char* bw, unsigned char* rgba, int total){
    for (int i=0; i < total; i += 4)
        bw[i/4] = (rgba[i] + rgba[i+1] + rgba[i+2]) / 3;
    return;
}

int count_targets(unsigned char* bw, unsigned char* blur, int w, int h, int limit){
    int total = w*h;
    int* vis = (int*)malloc(total*sizeof(int));
    for(int i=0; i < total; i++) 
        vis[i] = 0;

    int found = 0;
    int dx[4] = {-1, 1, 0, 0};
    int dy[4] = {0, 0, -1, 1};
    int q[20];

    for(int i=0; i < total; i++){
        if(bw[i] == 255 && vis[i] == 0){
            int head = 0, tail = 0, sz = 0, cx = 0, cy = 0;
            q[tail++] = i;
            vis[i] = 1;

            while(head < tail && sz <= limit){
                int cur = q[head++];
                int cy_cur = cur / w;
                int cx_cur = cur % w;
                sz++;
                cx += cx_cur;
                cy += cy_cur;

                for(int k=0; k < 4; k++){
                    int nx = cx_cur + dx[k];
                    int ny = cy_cur + dy[k];
                    if(nx >= 0 && nx < w && ny >= 0 && ny < h){
                        int nid = ny*w + nx;
                        if(vis[nid] == 0 && bw[nid] == 255){
                            vis[nid] = 1;
                            q[tail++] = nid;
                        }
                    }
                }
            }

            if(sz >= 2 && sz <= limit){
                cx /= sz;
                cy /= sz;

                int r = 6, dark_cnt = 0, total_cnt = 0, di, dj;
                for(di = -r; di <= r; di++){
                    for(dj = -r; dj <= r; dj++){
                        int nx = cx + dj, ny = cy + di;
                        if(nx >= 0 && nx < w && ny >= 0 && ny < h){
                            total_cnt++;
                            if(blur[ny*w + nx] < 65) dark_cnt++;
                        }
                    }
                }

                if(total_cnt > 0 && dark_cnt*100 / total_cnt > 70){
                    found++;
                }
            }
        }
    }
    free(vis);
    return found;
}

int main(void) {
    char filename[] = "tankers.png";
    unsigned int width, height;
    int size;
    int bw_size;

    unsigned char* picture = load_png(filename, &width, &height);
    if (picture == NULL){
        printf("Problem reading picture from the file %s. Error.\n", filename);
        return -1;
    }

    size = width * height * 4;
    bw_size = width * height;

    unsigned char* bw_pic = (unsigned char*)malloc(bw_size * sizeof(unsigned char));
    unsigned char* blr_pic = (unsigned char*)malloc(bw_size * sizeof(unsigned char));
    unsigned char* finish = (unsigned char*)malloc(size * sizeof(unsigned char));

    to_bw(bw_pic, picture, size);
    contrast(bw_pic, bw_size);
   
    for(int i=0; i < bw_size; i++) {
        finish[i*4]   = bw_pic[i];
        finish[i*4+1] = bw_pic[i];
        finish[i*4+2] = bw_pic[i];
        finish[i*4+3] = 255;
    }
    write_png("contrast.png", finish, width, height);

    Gauss_blur(bw_pic, blr_pic, width, height);
    for(int i=0; i < bw_size; i++) {
        finish[i*4]   = blr_pic[i];
        finish[i*4+1] = blr_pic[i];
        finish[i*4+2] = blr_pic[i];
        finish[i*4+3] = 255;
    }
    write_png("gauss.png", finish, width, height);

    int result = count_targets(bw_pic, blr_pic, width, height, 6);
    printf("Found tankers: %d\n", result);
    
    color(blr_pic, finish, bw_size);
    
    write_png("picture_out.png", finish, width, height);

    free(bw_pic);
    free(blr_pic);
    free(finish);
    free(picture);

    return 0;
} 