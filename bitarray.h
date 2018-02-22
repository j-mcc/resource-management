/*
 * $Author: o1-mccune $
 * $Date: 2017/11/11 01:44:56 $
 * $Revision: 1.1 $
 * $Log: bitarray.h,v $
 * Revision 1.1  2017/11/11 01:44:56  o1-mccune
 * Initial revision
 *
 */

#ifndef BITARRAY_H
#define BITARRAY_H

#define MAX_VALUE 255
#define MIN_VALUE 0

void clearAll();

void setAll();

void printByte(unsigned char byte);

int initBitVector(unsigned int size, unsigned char initialValue);

int getBit(unsigned int index);

int setBit(unsigned int index);

int flipBit(unsigned int index);

int clearBit(unsigned int index);

void printBitVector();

void freeBitVector();

int isFull();

int isEmpty();

#endif
