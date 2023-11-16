#ifndef __PARSE_H
#define __PARSE_H

#include<stdint.h>

typedef struct __enclosureInfo
{
  uint8_t * link;
  size_t length;
  uint8_t * type;
} enclosureInfo;

typedef struct __itemInfo
{
  uint8_t * title;
  uint8_t * link;
  uint8_t * description;
  // TODO add multiple-enclosure support
  enclosureInfo * enclosure;
} itemInfo;

void enclosureFreeInfo(enclosureInfo*);
void itemFreeInfo(itemInfo*);

typedef struct __channelInfo
{
  uint8_t * title;
  uint8_t * link;
  uint8_t * description;
  // TODO better storage of items in struct
  itemInfo ** items;
  size_t itemCount;
  size_t itemCapacity;
} channelInfo;

void channelFreeInfo(channelInfo*);

channelInfo * channelParseFd(int);
channelInfo * channelParseFname(char *);

#endif
