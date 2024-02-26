#include "recordFromFormat.h"
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

void printBits(uint16_t value) {
    for (int i = sizeof(value) * 8 - 1; i >= 0; i--) {
        uint16_t mask = 1U << i;
        uint16_t bit = (value & mask) ? 1 : 0;
        printf("%u", bit);
    }
    printf("\n");
}

int extractUsername(Record * r,char* buffer, int i, int username_length){
    char * value = malloc(sizeof(char) * username_length+1);
    memset(value,0,sizeof(char) * username_length+1);
    int j = 0;
    while(buffer[i] != '"'){
        value[j] = buffer[i];
        j++; i++;
    }
    value[j] = '\0';
    if(strlen(value) != 0){
        setUsername(r,value);
    }
    free(value);
    return i;
}

int extractId(Record * r,char* buffer, int i){
//* max value calculated from uint32_t 32 bit = 4,294,967,295 max value which has length 10+1 (nullbyte)
    char * value = malloc(sizeof(char)*10+1);
    memset(value,0,sizeof(char)*10+1);

    int j = 0;
    while(buffer[i] != '"' && j != 11){
        value[j] = buffer[i];
        j++; i++;
    }
    value[j] = '\0';
    if(strlen(value) != 0){
        setId(r, (uint32_t) strtoul(value,NULL,10));
    }
    free(value);
    return i;
}

int extractGroup(Record * r,char* buffer, int i){
//* max value calculated same as in extractID
    char * value = malloc(sizeof(char)*10+1);
    memset(value,0,sizeof(char)*10+1);

    int j = 0;
    while(buffer[i] != '"' && j != 11){
        value[j] = buffer[i];
        j++; i++;
    }
    value[j] = '\0';
    if(strlen(value) != 0){
        setGroup(r, (uint32_t) strtoul(value,NULL,10));
    }
    free(value);
    return i;
}

int extractSemester(Record * r,char* buffer, int i){
//* max value calculated from uint8_t 8 bit = 255 max int value which has length 3+1 (nullbyte)
    char * value = malloc(sizeof(char)*3+1);
    memset(value,0,sizeof(char)*3+1);

    int j = 0;
    while(buffer[i] != '"' && j != 4){
        value[j] = buffer[i];
        j++; i++;
    }
    value[j] = '\0';
    if(strlen(value) != 0){
        setSemester(r, (uint8_t) strtoul(value,NULL,10));
    }
    free(value);
    return i;
}

int extractGrade(Record * r,char* buffer, int i){
//* max value calculated same as in extractSemester
    char * value = malloc(sizeof(char)*8+1);
    memset(value,0,sizeof(char)*8+1);

    int j = 0;
    while(buffer[i] != '"' && j != 9){
        value[j] = buffer[i];
        j++; i++;
    }
    value[j] = '\0';
    if(strlen(value) != 0){
        if(value[0] == 'N') setGrade(r, Grade_None);
        else if(value[0] == 'B') setGrade(r, Grade_Bachelor);
        else if(value[0] == 'M') setGrade(r, Grade_Master);
        else if(value[0] == 'P') setGrade(r, Grade_PhD);
        else printf("invalid grade found in XML\n");
    }
    free(value);
    return i;
}


int extractCourse(Record * r,char* buffer, int i){
//* max value calculated through max value of a course, all courses have length 6+1 (null byte)
    char * value = malloc(sizeof(char)*(6+1));
    memset(value,0,sizeof(char)*(6+1));
    int j = 0;

    while(buffer[i] != '"' && j != 7){
        value[j] = buffer[i];
        j++; i++;
    }
    value[j] = '\0';
    if(value[3] == '0'){
        if(value[4] == '0'){
            setCourse(r,Course_IN1000);
        }
        if(value[4] == '1'){
            setCourse(r,Course_IN1010);
        }
        if(value[4] == '2'){
            setCourse(r,Course_IN1020);
        }
        if(value[4] == '3'){
            setCourse(r,Course_IN1030);
        }
        if(value[4] == '5'){
            setCourse(r,Course_IN1050);
        }
        if(value[4] == '6'){
            setCourse(r,Course_IN1060);
        }
        if(value[4] == '8'){
            setCourse(r,Course_IN1080);
        }
    }
    else if(value[3] == '1'){
        if(value[4] == '4'){
            setCourse(r,Course_IN1140);
        }
        if(value[4] == '5'){
            setCourse(r,Course_IN1150);
        }
    }
    else{
        if(value[4] == '0'){
            setCourse(r,Course_IN1900);
        }
        if(value[4] == '1'){
            setCourse(r,Course_IN1910);
        }
    }
    free(value);
    printBits(r-> courses);

    return i+7;
}

int extractCourses(Record * r,char* buffer, int i){
    memset(&(r -> courses),0,2);
    while(buffer[i] != 's' && buffer[i+1] != '>'){
        if(buffer[i] == '<' && buffer[i+1] != '/'){
            i = extractCourse(r,buffer, i+9);
        }
        i++;
    }
    printBits(r-> courses);
    return i;
}

Record* XMLtoRecord( char* buffer, int bufSize, int* bytesread )
{
    if(buffer[0] == '\0'){
        fprintf(stderr,"Buffer is empty in XMLtoRecord\n");
        *bytesread = 0;
        return NULL;
    }
    int i = 0;
    Record* record = newRecord();
    char isComplete = 'f';
    while(i < *bytesread){
        if(buffer[i] == '<'){
        //* an integer value is added on the index i in extractTAGNAME(buffer,i)
        //* in order to "skip" through the tagname due to better efficiency
            if(buffer[i+1] == 'r'){
                if(isComplete == 'f'){
                    isComplete = 'h';
                }
            }
            if(buffer[i+1] == 'u'){
                i = i+11;
            //* here is the length of the username extracted
            //* so that not too much memory is allocated in extractUsername
                int username_length = i;
                while(buffer[username_length] != '"'){
                    username_length++;
                }
                i = extractUsername(record,buffer, i, username_length-i);
            }
            if(buffer[i+1] == 'i'){
                i = extractId(record,buffer,i + 5); //? kanske malloca value i de olika funktionerna
            }
            if(buffer[i+1] == 'g' && buffer[i+3] == 'o'){
                i = extractGroup(record,buffer,i + 8);
            }
            if(buffer[i+1] == 's' && buffer[i+2] == 'e'){
                i = extractSemester(record,buffer,i + 11);
            }
            if(buffer[i+1] == 'g' && buffer[i+3] == 'a'){
                i = extractGrade(record,buffer,i + 8);
            }
            if(buffer[i+1] == 'c' && buffer[i+7] == 's'){
                i = extractCourses(record,buffer, i + 8);
            }
            if(buffer[i+1] == 'd'){
                setDest(record,buffer[i + 7]);
                i = i+ 7;
            }
            if(buffer[i+1] == 's' && buffer[i+2] == 'o'){
                setSource(record,buffer[i + 9]);
                i = i+ 9;

            }
    //* The following block handles the case where two record are sent at once (i.e. buffer contains two records).
    //* The parsed part of the buffer is set to 0
    //* so that it can be used to parse the next record of the buffer.
            if(buffer[i+2] == 'r'){
                while(buffer[i] != '>'){
                    i++;
                }
                memset(buffer,0,i);
                *bytesread = *bytesread-(i-1);
                if(isComplete == 'h'){
                    isComplete = 't';
                }
                break;
            }
        }
        i++;
    }
    if(isComplete == 't'){
        return record;
    }else{
        deleteRecord(record);
        fprintf(stderr,"Message did not consist of a complete record\n");
        return NULL;
    }
}



Record* BinaryToRecord( char* buffer, int bufSize, int* bytesread )
{
    uint32_t flags;
    if(buffer[0] == '\0'){
        fprintf(stderr,"Buffer is empty in BinarytoRecord\n");
        *bytesread = 0;
        return NULL;
    }
    if(buffer[0]){
        flags = buffer[0];
    }
    else{
        printf("flags = 0 in BinarytoRecord, record does not contains any relevant data\n");
        *bytesread = 1;
        return NULL;
    }

    int index = 1;
    uint32_t value;
    struct Record * record = newRecord();
    if (flags & FLAG_SRC) { //? && index < *bytesread
        value = 0;
        value = buffer[index++];
        setSource(record, (uint8_t)value);
    }

    if (flags & FLAG_DST) {
        value = 0;
        value = buffer[index++];
        setDest(record,(uint8_t) value);
    }

    if (flags & FLAG_USERNAME) {
        value = 0;
        int username_length;
        for(int i = 0; i < 4; i++){
            value |= ((uint32_t)buffer[index++] << (8 * i));
        }
        username_length = (int)ntohl(value);
        char * username = malloc(sizeof(char)*(username_length+1));
        for(int i = 0; i < username_length; i++){
            username[i] = (char) buffer[index++];
        }
        username[username_length] = '\0';
        setUsername(record, username);
        free(username);
    }

    if (flags & FLAG_ID) {
        value = 0;
        for(int i = 0; i < 4 ; i++){
            value |= ((uint32_t)buffer[index++] << (8 * i));
        }
        setId(record, ntohl(value));
    }

    if (flags & FLAG_GROUP) {
        value = 0;
        for(int i = 0; i < 4 ; i++){
            value |= ((uint32_t)buffer[index++] << (8 * i));
        }
        setGroup(record, ntohl(value));
    }

    if (flags & FLAG_SEMESTER ) {
        value = 0;
        value = buffer[index++];
        setSemester(record, (uint8_t)value);
    }

    if (flags & FLAG_GRADE) {
    // Flag 7 - grade 1 byte
        value = 0;
        value = buffer[index++];
        if(value == 3){
            setGrade(record, Grade_PhD);
        }
        else if(value == 2){
            setGrade(record, Grade_Master);
        }
        else if(value == 1){
            setGrade(record, Grade_Bachelor);
        }
        else{
            setGrade(record,Grade_None);
        }
    }

    if (flags & FLAG_COURSES) {
        value = 0;
        for (int i = 0; i < 2; i++) {
            //* For the reason that courses are not sent in network byte order, the following bit operation must take place.
            //* The first byte is pushed 8 bits to the left, set into value and then sets the second byte is set
            //* on the furthermost position to the right.
            if(i == 1){
                value |= ((uint8_t)buffer[index++]);
            }else{
                value |= ((uint8_t)buffer[index++] << 8);
            }
        }
        if((uint16_t)value & Course_IN1910){
            setCourse(record, Course_IN1910);
        }
        if((uint16_t)value & Course_IN1900){
            setCourse(record, Course_IN1900);
        }
        if((uint16_t)value & Course_IN1150){
            setCourse(record, Course_IN1150);
        }
        if((uint16_t)value & Course_IN1140){
            setCourse(record, Course_IN1140);
        }
        if((uint16_t)value & Course_IN1080){
            setCourse(record, Course_IN1080);
        }
        if((uint16_t)value & Course_IN1060){
            setCourse(record, Course_IN1060);
        }
        if((uint16_t)value & Course_IN1050){
            setCourse(record, Course_IN1050);
        }
        if((uint16_t)value & Course_IN1030){
            setCourse(record, Course_IN1030);
        }
        if((uint16_t)value & Course_IN1020){
            setCourse(record, Course_IN1020);
        }
        if((uint16_t)value & Course_IN1010){
            setCourse(record, Course_IN1010);
        }
        if((uint16_t)value & Course_IN1000){
            setCourse(record, Course_IN1000);
        }
    }
    memset(buffer,0,index);
    *bytesread = *bytesread-index;
    return record;
}

