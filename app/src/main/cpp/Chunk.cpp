//
// Created by aleksandar on 8/21/21.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "chunk.hpp"
#include "constants.h"

Chunk::Chunk(char* content, int length){
    recPack= (char*)malloc(length);
    memset(recPack, '\0', length);
    for(int i=0;i<length;i++)
        *(recPack+i)= *(content+i);
}

void Chunk::clear(){
    free(recPack);
}

Chunk::Chunk(char (&content)[CHUNK_SIZE+1024]){
    memset(recPack, '\0', CHUNK_SIZE+1024);
    memset(chunk, '\0', CHUNK_SIZE+1024);

    for(int i=0;i<CHUNK_SIZE+1024;i++)
        recPack[i]= content[i];
}

Chunk::Chunk(u_char (&content)[CHUNK_SIZE], char fileName[], long size, int order){
    memset(chunk, '\0', CHUNK_SIZE+1024);

int f_name_size= strlen(fileName);
int offset= 0;

this->addPaddingStart();
offset += 2;
this->addLimitator(offset, HEADER_START);
offset += 4;
this->addTransmissionType(CHUNK);
offset += 1;
this->addContentLength(size, offset);
offset += 8;
this->addFileNameSizeLength(f_name_size, offset);
offset += 4;
this->addFileName(fileName, offset);
offset+= f_name_size;
this->addChunkOrderNumber(offset, order);
offset += 4;
this->addLimitator(offset, HEADER_END);
offset += 4;
this->addLimitator(offset, CONTENT_START);
offset += 4;

for(int i=0;i<size;i++)
chunk[offset+i]= content[i];

offset+=size;

this->addLimitator(offset, CONTENT_END);
offset += 4;
this->addPaddingEnd(offset);
printf("");
}

void Chunk::addPaddingStart(){
    chunk[0]= 0x01;
    chunk[1]= 0x02;
}

void Chunk::addLimitator(int offset, const char type[]){
    for(int i=1; i<=4;i++){
        if(i % 4 != 0)
            chunk[offset + i -1]= 0x00;
        else
            chunk[offset + i -1]= type[0];
    }
}

void Chunk::addTransmissionType(const char type[]){
    chunk[6]= type[0];
}

void Chunk::addContentLength(long dataLength, int offset){
    char* l= (char*)&dataLength;
    for(int i=1;i<=8;i++)
        chunk[offset+i-1]= l[i-1];
}

void Chunk::addFileNameSizeLength(int f_name_length, int offset){
    char* l= (char*)&f_name_length;
    for(int i=1;i<=4;i++)
        chunk[offset+i-1]= l[i-1];
}

void Chunk::addFileName(char f_name[], int offset){
    for(int i=0;i< strlen(f_name);i++)
        chunk[offset+i]= f_name[i];
}

char* Chunk::getFileName(){
    char to_int[] = { chunk[15], chunk[16], chunk[17], chunk[18] };
    int* i= (int*)&to_int;
    int num= *i;

    char n[num+1];
    for(int o=0; o<*i;o++)
        n[o]= chunk[19+o];

    n[num]='\0';

    char* name= (char*)malloc(*i+1);
    strcpy(name, n);

    return name;
}

void Chunk::addChunkOrderNumber(int offset, int order){
    char* num=(char*)&order;

    for(int i=0;i<4;i++)
        chunk[offset+i]= num[i];

}

void Chunk::addPaddingEnd(int offset){
    chunk[offset]= 0x01;
    chunk[offset+1]= 0x02;
}

void Chunk::getBytes(char* target){
    for(int i=0;i<CHUNK_SIZE+1024;i++)
        target[i]= chunk[i];
}

int Chunk::getContentLength(){
    char num[]={recPack[7],
                recPack[8],
                recPack[9],
                recPack[10],
                recPack[11],
                recPack[12],
                recPack[13],
                recPack[14]};
    int* ret=(int*)&num;
    return *ret;
}

int Chunk::getOrderNumber(){
    char to_int[] = { recPack[15],
                      recPack[16],
                      recPack[17],
                      recPack[18] };
    int* i= (int*)&to_int;
    int size= *i;

    size+= 19;
    char to_num[] = { recPack[size],
                      recPack[size+1],
                      recPack[size+2],
                      recPack[size+3]};
    int* no= (int*)&to_num;

    return *no;
}

void Chunk::getContent(char *content){
    char to_int[] = { recPack[15],
                      recPack[16],
                      recPack[17],
                      recPack[18] };
    int* i= (int*)&to_int;
    int size= *i;

    int length= getContentLength();

    size+=31;
    for(int i=0;i<length;i++)
        content[i]=recPack[size+i];
}