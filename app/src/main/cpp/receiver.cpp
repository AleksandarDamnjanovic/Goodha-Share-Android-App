//
// Created by aleksandar on 9/26/21.
//

#include <jni.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <strings.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "chunk.hpp"
#include <sys/time.h>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "constants.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

struct report{
    int socket;
    char* fileName;
    char* relativePath;
    long length;
    long transferred;
    char ip[16];
    pthread_t thread;
};

void* engine(void* ptr);
void* comThread(void* client);

void resend(int* conf, int order_num);
void accepted(int* conf, int order);

#define SA struct sockaddr
int listener;
pthread_t generator;
pthread_mutex_t lock;
bool run= true;
bool receiving= false;
bool cancelation= false;
std::vector<report*>clients;
std::vector<char*>allowed;
std::vector<char*>blocked;
char buff[CHUNK_SIZE+1024];
char* r_path;

void* engine(void* ptr){

    int* listener= (int*)ptr;
    while(run){
        bool jump= false;
        struct sockaddr_in addr;
        socklen_t addr_len;

        int conf= accept(*listener,(SA*) &addr, &addr_len);

        char str[INET_ADDRSTRLEN];
        memset(str, '\0', INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &addr.sin_addr, str, INET_ADDRSTRLEN);

        if(allowed.size()!=0){
            bool free= false;
            for(char* s: allowed){
                if(strcmp(s, str)==0){
                    free=true;
                    goto sol;
                }
            }

            sol:
            if(!free){
                close(conf);
                jump= true;;
            }

        }else if(blocked.size()!=0){
            for(char* s: blocked)
                if(strcmp(s, str)==0){
                    close(conf);
                    jump= true;
                }
        }

        if(!jump){
            pthread_t thread;
            report* r=(report*)malloc(sizeof(report));
            r->socket=conf;
            strcpy(r->ip, str);
            r->thread= thread;
            clients.push_back(r);
            pthread_create(&thread, NULL, comThread, (void*)&conf);
        }
        jump= false;
    }

    return NULL;
}

void* comThread(void* client){
    int conf= ((struct report*)client)->socket;

    report* r;

    pthread_mutex_lock(&lock);
    for(int i=0;i<clients.size();i++)
        if(clients[i]->socket== conf)
            r=clients[i];
    pthread_mutex_unlock(&lock);

    char mess[1024];
    recv(conf, mess, 1024, MSG_WAITALL);

    char *ptr;
    char c_len[8]={mess[0], mess[1], mess[2], mess[3], mess[4], mess[5], mess[6], mess[7]};
    char fn_len[8]={mess[10], mess[11], mess[12], mess[13], mess[14], mess[15], mess[16], mess[17]};
    char rp_len[8]={mess[20], mess[21], mess[22], mess[23], mess[24], mess[25], mess[26], mess[27]};

    long* content_length= (long*)&c_len;
    long* fn_length= (long*)&fn_len;
    long* rp_length= (long*)&rp_len;

    if(*content_length==0 || *fn_len==0 || rp_len==0){

        memset(buff, '\0', CHUNK_SIZE + 1024);

        int e=-1;
        pthread_mutex_lock(&lock);
        printf("%s\t%s\tcommunication broken\n", r->relativePath, r->fileName);
        for(int i=0;i<clients.size();i++)
            if(clients[i]->socket== conf)
                e= i;

        pthread_mutex_unlock(&lock);

        if(e!=-1)
            clients.erase(clients.begin()+e);

        close(conf);
        free(r);
        pthread_exit(NULL);
    }

    char fileName[*fn_length+1];
    memset(fileName,'\0', strlen(fileName));
    for(int i=0;i<*fn_length;i++)
        fileName[i]= mess[30+i];

    fileName[*fn_length]='\0';

    if(fileName[strlen(fileName)-1]=='/' || fileName[strlen(fileName)-1]=='\\')
        fileName[strlen(fileName)-1]='\0';

    char relativePath[*rp_length+1];
    memset(relativePath, '\0', *rp_length+1);
    for(int i=0;i<*rp_length;i++)
        relativePath[i]= mess[32+*fn_length+i];

    relativePath[*rp_length]='\0';

    pthread_mutex_lock(&lock);
    r->relativePath= relativePath;
    r->fileName= fileName;

    char f_name[(r_path==NULL?1:strlen(r_path)*sizeof(char))
                +strlen(r->relativePath)*sizeof(char)
                +strlen(r->fileName)*sizeof(char)];

    memset(f_name,'\0',strlen(f_name)*sizeof(char));
    strcpy(f_name, r_path==NULL?".":r_path);
    strcat(f_name, r->relativePath);
    strcat(f_name, r->fileName);
    pthread_mutex_unlock(&lock);

    bool win= strstr(f_name, "/")==NULL?true:false;
    int counter= 0;

    char limit[sizeof(char)*2];
    if(win)
        strcpy(limit, "\\");
    else
        strcpy(limit, "/");

    strcat(limit,"\0");

    for(int i=0; i<strlen(f_name);i++){
        if(limit[0]==f_name[i]){
            counter= i;
            char part[i+1];
            memset(part, '\0', strlen(part));
            for(int o=0;o<counter;o++)
                part[o]=f_name[o];

            part[counter]='\0';

            if(access(part, F_OK)!=0)
                mkdir(part,0777);
        }

    }

    remove(f_name);
    FILE *f;
    f= fopen(f_name,"ab");
    int n;
    int suma=0;
    long size= CHUNK_SIZE+1024;

    pthread_mutex_lock(&lock);
    r->length= *content_length;
    pthread_mutex_unlock(&lock);

    int order_num=0;
    int order=0;
    loop:
    order=0;
    memset(buff,'\0',CHUNK_SIZE+1024);

    pthread_mutex_lock(&lock);
    receiving= true;
    pthread_mutex_unlock(&lock);

    while((n= recv(conf, buff, CHUNK_SIZE+1024, MSG_WAITALL))>0){

        Chunk chunk(buff, CHUNK_SIZE+1024);

        if(buff[0]!=0x01 && buff[1]!=0x02){
            resend(&conf, order_num);
            goto loop;
        }

        order= chunk.getOrderNumber();

        if(order!= order_num){
            resend(&conf, order_num);
            goto loop;
        }

        int contentLength= chunk.getContentLength();

        if(contentLength==0){
            resend(&conf, order_num-1);
            goto loop;
        }

        if(*content_length<CHUNK_SIZE+1024)
            contentLength=*content_length;
        char finalChunk[contentLength];
        chunk.getContent(finalChunk);

        int sent= fwrite(finalChunk, contentLength, sizeof(char), f);
        suma+=contentLength;

        accepted(&conf, order);

        if(*content_length-suma<CHUNK_SIZE+1024)
            size=*content_length-suma;
        else
            size=CHUNK_SIZE+1024;

        order_num+=1;

        pthread_mutex_lock(&lock);
        r->transferred= suma;
        pthread_mutex_unlock(&lock);

        if(*content_length<=suma)
            goto end;

        chunk.clear();

        pthread_mutex_lock(&lock);
        if(cancelation){
            pthread_mutex_unlock(&lock);
            goto end;
        }else
            pthread_mutex_unlock(&lock);

    }

    end:
    pthread_mutex_lock(&lock);
    cancelation=false;
    receiving= false;
    pthread_mutex_unlock(&lock);
    fclose(f);
    memset(buff, '\0', 1024);

    int e=-1;
    pthread_mutex_lock(&lock);
    printf("%s\t%s\ttransfer completed\n", r->relativePath, r->fileName);
    for(int i=0;i<clients.size();i++)
        if(clients[i]->socket== conf)
            e= i;

    pthread_mutex_unlock(&lock);

    if(e!=-1)
        clients.erase(clients.begin()+e);

    close(conf);
    free(r);
    pthread_exit(NULL);
}

void resend(int* conf, int order_num){
    char response[12];
    response[0]=0x0b;
    response[1]=0x0b;
    response[2]=0x0b;
    response[3]=0x0b;
    char* l= (char*)&order_num;
    response[4]=l[0];
    response[5]=l[1];
    response[6]=l[2];
    response[7]=l[3];
    response[8]=0x0b;
    response[9]=0x0b;
    response[10]=0x0b;
    response[11]=0x0b;
    write(*conf, response, 12);
}

void accepted(int* conf, int order){
    char response[12];
    response[0]=0x0a;
    response[1]=0x0a;
    response[2]=0x0a;
    response[3]=0x0a;
    char* l= (char*)&order;
    response[4]=l[0];
    response[5]=l[1];
    response[6]=l[2];
    response[7]=l[3];
    response[8]=0x0a;
    response[9]=0x0a;
    response[10]=0x0a;
    response[11]=0x0a;
    write(*conf, response, 12);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_receiverStart(JNIEnv *env, jobject thiz, jstring relative_path) {

    const jsize len= env->GetStringUTFLength(relative_path);
    const char* strChar= env->GetStringUTFChars(relative_path, (jboolean *)0);
    std::string Result(strChar, len);
    env->ReleaseStringUTFChars(relative_path, strChar);

    r_path=(char*)malloc(len+1);
    memset(r_path, '\0', len+1);
    strcpy(r_path, Result.c_str());

    struct sockaddr_in server_address;

    if((listener= socket(AF_INET, SOCK_STREAM, 0))<0)
        run= false;

    bzero(&server_address, sizeof(server_address));
    server_address.sin_family= AF_INET;
    server_address.sin_addr.s_addr= htonl(INADDR_ANY);
    server_address.sin_port= htons(SERVICE_PORT);

    if((bind(listener, (SA*) &server_address, sizeof(server_address)))<0)
        run=false;

    if((listen(listener, 10))!=0)
        run=false;

    printf("Into the loop:\n");

    pthread_create(&generator, NULL, engine, (void*)&listener);

    while (run)
        sleep(0.5);

}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_getPresentlyReceiving(JNIEnv *env,
                                                                                  jobject thiz) {
    jboolean ret= false;
    pthread_mutex_lock(&lock);
    ret= receiving;
    pthread_mutex_unlock(&lock);
    return ret;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_cancelReceiving(JNIEnv *env,
                                                                            jobject thiz) {
    cancelation= true;
    receiving= false;
}
extern "C"
JNIEXPORT jstring JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_getReceiverState(JNIEnv *env, jobject thiz) {
    std::string content="";

    pthread_mutex_lock(&lock);
    for(int i=0;i<clients.size();i++){
        char ret[strlen(clients[i]->ip) * sizeof(char) +
        strlen(clients[i]->fileName) * sizeof(char) + 30 ];

        sprintf(ret, "%s|||||%s|||||%d|||||%d\n", clients[i]->ip, clients[i]->fileName,
                clients[i]->transferred, clients[i]->length);

        content+=ret;
    }
    pthread_mutex_unlock(&lock);

    return env->NewStringUTF(content.c_str());
}