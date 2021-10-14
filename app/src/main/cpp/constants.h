//
// Created by aleksandar on 8/21/21.
//

#ifndef GOODHA_SHARE_CONSTANTS_H
#define GOODHA_SHARE_CONSTANTS_H

const int CHUNK_SIZE= 1024*50;

const char HEADER_START[1]={0x00};
const char HEADER_END[1]={0x01};
const char CONTENT_START[1]={0x02};
const char CONTENT_END[1]={0x03};

const char CHUNK[1]={0x0d};
const char CHUNK_RECEIVED[1]={0x0a};

const int SERVICE_PORT= 44445;
#define SERVICE_PORT_CHAR "44445"

const int DETECTION_PORT= 44446;
#define DETECTION_PORT_CHAR "44446"

#endif //GOODHA_SHARE_CONSTANTS_H