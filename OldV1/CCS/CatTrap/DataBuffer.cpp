/*
 * DataBuffer.cpp
 *
 *  Created on: Feb 15, 2017
 *      Author: Ivan
 */

#include "DataBuffer.h"

DataBuffer::DataBuffer()
{
    bufferCounter = 0;
    bufferSize = BUFFERSIZE;

}

DataBuffer::~DataBuffer()
{
    // TODO Auto-generated destructor stub
}

void DataBuffer::Reset()
{
    bufferCounter = 0;
}

bool DataBuffer::AddData(unsigned short data)
{
    if( bufferCounter < bufferSize)
    {
        Buffer[bufferCounter] = data;
        bufferCounter++;

        return true;
    }

    return false;
}

int DataBuffer::GetBufferPosition()
{
    return bufferCounter;
}

void DataBuffer::GetBufferData(unsigned short* data, int offset, int count)
{
    for(int i=0; i!=count; i++)
    {
        int index = offset + i;

        if( index < bufferSize)
        {
            data[i] = Buffer[index];
        }
    }
}
