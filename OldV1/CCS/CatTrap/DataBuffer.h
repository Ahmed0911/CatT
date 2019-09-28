/*
 * DataBuffer.h
 *
 *  Created on: Feb 15, 2017
 *      Author: Ivan
 */

#ifndef DATABUFFER_H_
#define DATABUFFER_H_

#define BUFFERSIZE 50000

class DataBuffer
{
public:
    DataBuffer();
    virtual ~DataBuffer();

public:
    void Reset();
    bool AddData(unsigned short data);
    int GetBufferPosition();
    void GetBufferData(unsigned short* data, int offset, int count);

private:
    unsigned short Buffer[BUFFERSIZE];
    int bufferCounter;
    int bufferSize;
};

#endif /* DATABUFFER_H_ */
