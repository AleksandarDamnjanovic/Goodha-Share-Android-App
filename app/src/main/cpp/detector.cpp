//
// Created by aleksandar on 9/20/21.
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
#include "constants.h"
#include <sys/socket.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <algorithm>

struct forSender{
    int sock;
    sockaddr_in address;
};

void* terminal(void* ptr);
void* receiver_function(void* ptr);
void* sender(void* sock);
pthread_t receiver;
std::vector<std::string> servives;
pthread_mutex_t lock;

bool receiving= true;
bool communicating= true;
bool completed=false;
int mainSocketID;
char* hostName;
extern "C"
JNIEXPORT void JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_initializeDetector(JNIEnv *env, jobject thiz, jstring device) {

    const jsize len= env->GetStringUTFLength(device);
    const char* strChar= env->GetStringUTFChars(device, (jboolean *)0);
    std::string Result(strChar, len);
    env->ReleaseStringUTFChars(device, strChar);

    hostName= (char*)malloc(sizeof(char) * strlen(Result.c_str()));
    memset(hostName, '\0', sizeof(char) * strlen(Result.c_str()));
    strcpy(hostName, Result.c_str());

    pthread_create(&receiver, NULL, receiver_function, NULL);

}

void* receiver_function(void* ptr){

    char buffer[512];
    struct sockaddr_in serverAddress, clientAddress;

    mainSocketID= socket(AF_INET, SOCK_DGRAM,0);
    memset(&serverAddress, '\0', sizeof(serverAddress));

    serverAddress.sin_family= AF_INET;
    serverAddress.sin_addr.s_addr= INADDR_ANY;
    serverAddress.sin_port= htons(DETECTION_PORT);
    bind(mainSocketID, (const struct sockaddr*)&serverAddress, sizeof(serverAddress));

    while(receiving){
        memset(&clientAddress, '\0', sizeof(clientAddress));
        int len= sizeof(clientAddress);
        int n= recvfrom(mainSocketID, (char*)buffer, 512, MSG_WAITALL,
                        (struct sockaddr*)&clientAddress, (socklen_t*)&len);
        buffer[n]='\0';

        char str[INET_ADDRSTRLEN];
        memset(str, '\0', INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &clientAddress.sin_addr, str, INET_ADDRSTRLEN);

        char record[512];
        memset(record, '\0', 512);
        strcpy(record, str);
        strcat(record, " ");

        char* name= strtok(buffer," ");
        name= strtok(NULL, " ");

        strcat(record, name);

        if((buffer[0]=='a' && buffer[1]=='s' && buffer[2]=='k') ||
           (buffer[0]=='a' && buffer[1]=='l' && buffer[2]=='i' && buffer[3]=='v' && buffer[4]=='e'))
            if(!std::count(servives.begin(), servives.end(), record)){
                pthread_mutex_lock(&lock);
                servives.push_back(record);
                pthread_mutex_unlock(&lock);

                if(buffer[0]=='a' && buffer[1]=='s' && buffer[2]=='k'){
                    char alive[6 + strlen(hostName)];
                    strcpy(alive, "alive ");
                    strcat(alive, hostName);

                    int sock;
                    sock= socket(AF_INET, SOCK_DGRAM, 0);

                    struct sockaddr_in addr;
                    addr.sin_family = AF_INET;
                    addr.sin_port= htons(DETECTION_PORT);
                    addr.sin_addr.s_addr = inet_addr(str);

                    for(int i=0;i<3;i++){
                        sendto(sock, (const char *)alive, strlen(alive),
                               MSG_CONFIRM, (const struct sockaddr *) &addr,
                               sizeof(addr));
                    }

                    close(sock);
                }

            }

    }

    free(hostName);
    close(mainSocketID);

    return NULL;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_detect(JNIEnv *env, jobject thiz) {
    int socketID;
    struct sockaddr_in serverAddress;

    socketID= socket(AF_INET, SOCK_DGRAM,0);

    if(socketID<0){
        close(socketID);
    }else{
        forSender* sn;
        sn=(forSender*)malloc(sizeof(forSender));
        sn->sock= socketID;
        sn->address=serverAddress;

        pthread_t send_thread;
        pthread_create(&send_thread, NULL, &sender, sn);
    }
}

int scanned=0;
void* sender(void* sock){
    scanned=0;

    int socketID=((struct forSender*)sock)->sock;
    sockaddr_in serverAddress=((struct forSender*)sock)->address;

    int a3=0, a4=0;
    char ask[5+ strlen(hostName)];
    memset(ask, '\0', strlen(ask));
    strcpy(ask, "ask ");
    strcat(ask, hostName);

    pthread_mutex_lock(&lock);
    completed= false;
    pthread_mutex_unlock(&lock);

    printf("Scanning commenced\n");

    pthread_mutex_lock(&lock);
    while(!completed){
        pthread_mutex_unlock(&lock);
        char ip[16];
        memset(ip, '\0', 16);
        sprintf(ip,"%d.%d.%d.%d", 192, 168, a3, a4);
        serverAddress.sin_family= AF_INET;
        serverAddress.sin_addr.s_addr= inet_addr(ip);
        serverAddress.sin_port= htons(DETECTION_PORT);

        for(int i=0;i<3;i++){
            sendto(socketID, (const char *)ask, strlen(ask),
                   MSG_CONFIRM, (const struct sockaddr *) &serverAddress,
                   sizeof(serverAddress));
        }

        a4+=1;
        if(a4==256){
            a3+=1;
            a4=0;
        }

        if(a3==256){
            pthread_mutex_lock(&lock);
            completed=true;
            pthread_mutex_unlock(&lock);
        }

        scanned+=1;
        pthread_mutex_lock(&lock);
    }
    pthread_mutex_unlock(&lock);

    free(sock);
    return NULL;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_getDetectionRunning(JNIEnv *env,
                                                                                jobject thiz) {
    jboolean ret=false;
    pthread_mutex_lock(&lock);
    if(completed)
        ret=false;
    else
        ret= true;
    pthread_mutex_unlock(&lock);

    return ret;
}
extern "C"
JNIEXPORT jstring JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_getList(JNIEnv *env, jobject thiz) {
    pthread_mutex_lock(&lock);
    long len=0;

    for(int i=0;i<servives.size();i++)
        len+= (strlen(servives[i].c_str())+1) * sizeof(char);

    len+=1;
    char ret[len];
    memset(ret, '\0', len);

    for(int i=0;i<servives.size();i++){
        strcat(ret, servives[i].c_str());
        strcat(ret, (const char*)"\n");
    }

    pthread_mutex_unlock(&lock);

    return env->NewStringUTF(ret);
}
extern "C"
JNIEXPORT jlong JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_getScannedCount(JNIEnv *env,
                                                                            jobject thiz) {
    jlong ret=0;
    pthread_mutex_lock(&lock);
    ret= scanned;
    pthread_mutex_unlock(&lock);
    return ret;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_aleksandar_1damnjanovic_goodha_1share_MainActivity_cancelScanning(JNIEnv *env,
                                                                           jobject thiz) {
    completed= true;
}