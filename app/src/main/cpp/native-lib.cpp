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

#define SA struct sockaddr

void* waitForInput(void* action);
pthread_t t;
bool cancelation= false;
pthread_mutex_t lock;

bool running=true;
long length= 0;
long fullOffset= 0;
double elapsed;
struct timeval t1;
struct timeval t2;
long tempOffset=0;
long timeBuf=0;

char* ip;
char* localPath;
char* fileName;
char* relativePath;

void freeAll(){
    length=0;
    fullOffset=0;
    tempOffset=0;
    free(ip);
    free(localPath);
    free(fileName);
    free(relativePath);
}

char* getCharArray(JNIEnv *env, jstring &src){
    char* dest;
    const jsize len= env->GetStringUTFLength(src);
    const char* strChar= env->GetStringUTFChars(src, (jboolean *)0);
    std::string Result(strChar, len);
    env->ReleaseStringUTFChars(src, strChar);
    const char* c= Result.c_str();
    int fullLength=strlen(c) * sizeof(char);
    dest= (char*)malloc(fullLength);
    memset(dest, '\0', fullLength);
    strcpy(dest, c);
    return dest;
}


extern "C" JNIEXPORT jstring JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());

}

extern "C" JNIEXPORT jstring JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_bre(JNIEnv *env, jobject thiz, jstring str) {
    const jsize len= env->GetStringUTFLength(str);
    const char* strChar= env->GetStringUTFChars(str, (jboolean *)0);
    std::string Result(strChar, len);
    env->ReleaseStringUTFChars(str, strChar);
    Result+=" hi!";
    return env->NewStringUTF(Result.c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_send(JNIEnv *env, jobject thiz,
                                                                 jstring ip_src, jstring localPath_src,
                                                                 jstring fileName_src,
                                                                 jstring relative_path_src) {
    pthread_mutex_lock(&lock);
    running= true;
    cancelation= false;
    timeBuf=0;
    ip= getCharArray(env, ip_src);
    localPath= getCharArray(env, localPath_src);
    fileName= getCharArray(env, fileName_src);
    relativePath= getCharArray(env, relative_path_src);
    pthread_mutex_unlock(&lock);

    pthread_create(&t, NULL, waitForInput, NULL);

    while(running)
        sleep(0.5);

}

void* waitForInput(void* action){

    int socket_id;
    struct sockaddr_in server_address;
    u_char buff[CHUNK_SIZE];
    int hc_size= 200 * CHUNK_SIZE;
    u_char* hugeBuff=(u_char*)malloc(hc_size*sizeof(u_char));

    if((socket_id = socket(AF_INET, SOCK_STREAM, 0))<0){
        printf("Cannot create socket...\n");
        running=false;
        freeAll();
        pthread_exit(NULL);
    }

    bzero(&server_address, sizeof(server_address));
    server_address.sin_family= AF_INET;
    server_address.sin_port= htons(SERVICE_PORT);
    server_address.sin_addr.s_addr = inet_addr(ip);

    if(connect(socket_id, (SA*)&server_address, sizeof(server_address)) != 0){
        printf("Cannot connect...\n");
        running=false;
        freeAll();
        pthread_exit(NULL);
    }

    FILE *f;

    pthread_mutex_lock(&lock);
    f= fopen(localPath,"r");

    if(f==NULL){
        close(socket_id);
        freeAll();
        running=false;
        pthread_exit(NULL);
    }

    fseek(f,0,SEEK_END);
    length= ftell(f);
    pthread_mutex_unlock(&lock);

    fseek(f, 0,SEEK_SET);
    fclose(f);

    u_char mess[1024];

    pthread_mutex_lock(&lock);
    long fn_len= strlen(fileName);
    long rp_len= strlen(relativePath);
    char* v1= (char*)&length;

    char* v2= (char*)&fn_len;
    char* v3= (char*)&rp_len;

    for(int i=0;i<8;i++)
        mess[i]=v1[i];

    mess[8]=0x02;
    mess[9]=0x02;

    for(int i=0;i<8;i++)
        mess[i+10]=v2[i];

    mess[18]=0x02;
    mess[19]=0x02;

    for(int i=0;i<8;i++)
        mess[i+20]=v3[i];

    mess[28]=0x02;
    mess[29]=0x02;

    for(int i=0;i<fn_len;i++)
        mess[i+30]=fileName[i];

    mess[30+fn_len]=0x02;
    mess[31+fn_len]=0x02;

    for(int i=0;i<rp_len;i++)
        mess[i+32+fn_len]=relativePath[i];

    write(socket_id, mess, 1024);

    int order=0;
    int cur;
    int suma=0;
    int r= CHUNK_SIZE;
    long ll= length;
    long offset=0;
    long s_len= hc_size;

    if(s_len > length)
        s_len= length;

    f= fopen(localPath,"rb");
    pthread_mutex_unlock(&lock);

    while((cur = fread(hugeBuff, s_len, sizeof(char),f) >0)){

        int chunk_num= s_len / CHUNK_SIZE;
        if(s_len % CHUNK_SIZE>0)
            chunk_num+=1;

        offset=0;
        for(int i=0; i<chunk_num;i++){
            memset(buff, '\0', CHUNK_SIZE);
            memcpy(buff,hugeBuff+offset, (s_len-offset>CHUNK_SIZE?CHUNK_SIZE:s_len-offset));
            offset+=CHUNK_SIZE;
            tempOffset+=CHUNK_SIZE;

            start:Chunk chunk(buff, (char*)"name", r, order);

            if(cancelation)
                goto end;

            char pack[CHUNK_SIZE+1024];
            memset(pack, '\0', CHUNK_SIZE+1024);
            chunk.getBytes(pack);

            int writen= write(socket_id, pack, CHUNK_SIZE+1024);

            char response[12];
            recv(socket_id, response, 12, MSG_WAITALL);

            if(response[0]!=0x0a
               && response[1]!=0x0a
               && response[2]!=0x0a
               && response[3]!=0x0a)
                goto start;

            char to_int[4]={response[4], response[5], response[6], response[7]};
            int* res= (int*)&to_int;
            if(*res!=order)
                goto start;

            suma+= r;
            order+= 1;

            if(ll>r)
                ll-=r;
            else
                ll=0;

            if(ll <= CHUNK_SIZE)
                r=ll;
            else
                r= CHUNK_SIZE;

        }

        pthread_mutex_lock(&lock);
        fullOffset+= offset;

        if(length-fullOffset >= hc_size)
            s_len= hc_size;
        else
            s_len= length-fullOffset;

        if(s_len<=0)
            s_len=0;

        pthread_mutex_unlock(&lock);

        memset(hugeBuff, '\0', hc_size);
    }

    end:
    free(hugeBuff);
    fclose(f);
    running=false;
    close(socket_id);
    freeAll();
    running= false;
    pthread_exit(NULL);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_getTransferred(JNIEnv *env, jobject thiz) {
    long ret= 0;
    pthread_mutex_lock(&lock);
    ret= fullOffset;
    pthread_mutex_unlock(&lock);
    return ret;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_getLength(JNIEnv *env, jobject thiz) {
    long ret= 0;
    pthread_mutex_lock(&lock);
    ret= length;
    pthread_mutex_unlock(&lock);
    return ret;
}

extern "C"
JNIEXPORT jdouble JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_getSpeed(JNIEnv *env, jobject thiz) {
    gettimeofday(&t2, 0);

    elapsed= (t2.tv_sec - t1.tv_sec) * 1000.0f + (t2.tv_usec - t1.tv_usec) / 1000.0f;
    elapsed= elapsed/1000;

    timeBuf= tempOffset- timeBuf;

    double  ret=0;
    if(elapsed!=0)
        ret= timeBuf / elapsed;

    ret= ret/(1024*1024);

    timeBuf= tempOffset;

    gettimeofday(&t1, 0);

    return jdouble (ret);
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_getRunning(JNIEnv *env, jobject thiz) {
    jboolean ret = (jboolean)running;
    return ret;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_cancelSending(JNIEnv *env,
                                                                          jobject thiz) {
    cancelation= true;
    pthread_join(t, NULL);
}