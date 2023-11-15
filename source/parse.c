#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>

#include<libxml/parser.h>
#include<libxml/xmlreader.h>

#include "parse.h"
#include "logging.h"

void channelFreeInfo(channelInfo * channel)
{
  if (channel->title)
  {
    free(channel->title);
    channel->title = NULL;
  }
  if (channel->link)
  {
    free(channel->link);
    channel->link = NULL;
  }
  if (channel->description)
  {
    free(channel->description);
    channel->description = NULL;
  }
  if (channel->items)
  {
    for (int i = 0; i < channel->itemCount; ++i)
    {
      free(channel->items[i]);
    }
    free(channel->items);
    channel->items = NULL;
    channel->itemCount = 0;
  }
}

int parseTextContainer(xmlTextReader * reader, char** dest)
{
  xmlChar * name = xmlTextReaderName(reader);
  xmlChar * oname;
  if (xmlTextReaderIsEmptyElement(reader))
  {
    // If we're empty just skip
    return 0;
  }

  xmlTextReaderRead(reader);
  if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_TEXT)
  {
    if (*dest)
    {
      free(*dest);
    }
    *dest = xmlTextReaderReadString(reader);
  }
  else if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_CDATA)
  {
    if (*dest)
    {
      free(*dest);
    }
    *dest = xmlTextReaderValue(reader);
  }
  else if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT)
  {
    // we're closing so do nothing
  }
  else
  {
    // Bad text type
    free(name);
    return -1;
  }

  while (xmlTextReaderRead(reader) > 0)
  {
    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT)
    {
      oname = xmlTextReaderName(reader);
      if (strcmp(name, oname))
      {
        free(oname);
        free(name);
        return 0;
      }
      else
      {
        free(oname);
      }
    }
  }
  free(name);
  return -1;
}

// Parse an element of Channel which is to be brought into the Channel rather than an Item
void parseChannelSubElement(xmlTextReader * reader, channelInfo * mod)
{
  // Assume that reader is on an Element node
  // Now we determine how to handle that node
  xmlChar * name = xmlTextReaderName(reader);
  xmlChar * ns = xmlTextReaderPrefix(reader);
  if (strcmp(name, "title") == 0)
  {
    //printf("title namespace is %s\n", ns);
    parseTextContainer(reader, &mod->title);
    writelog_l(LOGLEVEL_DEBUG, "found channel title %s", mod->title);
  }
  else if (strcmp(name, "link") == 0)
  {
    parseTextContainer(reader, &mod->link);
    writelog_l(LOGLEVEL_DEBUG, "found channel link %s", mod->link);
  }
  else if (strcmp(name, "description") == 0)
  {
    parseTextContainer(reader, &mod->description);
    writelog_l(LOGLEVEL_DEBUG, "found channel description %.10s", mod->description);
  }
  free(ns);
  free(name);
}

int parseItemSubElement(xmlTextReader * reader, channelInfo * mod)
{
  itemInfo * item = malloc(sizeof(itemInfo));
  memset(item, 0, sizeof(itemInfo));

  // Stack variables to handle volatile responses
  xmlChar * name;
  xmlChar * ns;
  bool cont = true;
  int depth = xmlTextReaderDepth(reader);
  while (cont && xmlTextReaderRead(reader) > 0)
  {
    switch (xmlTextReaderNodeType(reader))
    {
      case XML_READER_TYPE_ELEMENT:
        name = xmlTextReaderName(reader);
        if (strcmp(name, "title") == 0)
        {
          parseTextContainer(reader, &item->title);
        }
        else if (strcmp(name, "description") == 0)
        {
          parseTextContainer(reader, &item->description);
        }
        else if (strcmp(name, "link") == 0)
        {
          parseTextContainer(reader, &item->link);
        }
        free(name);
        break;

      case XML_READER_TYPE_END_ELEMENT:
        if (depth == xmlTextReaderDepth(reader))
        {
          cont = false;
        }
        break;
    }
  }

  // Grow list if we need
  if (mod->itemCount == mod->itemCapacity)
  {
    if (mod->itemCapacity == 0)
    {
      mod->itemCapacity = 16;
      mod->items = malloc(sizeof(itemInfo*) * mod->itemCapacity);
    }
    else
    {
      int newCap = mod->itemCapacity * 2;
      itemInfo ** newList = malloc(sizeof(itemInfo*) * newCap);

      memcpy(newList, mod->items, sizeof(itemInfo*) * mod->itemCapacity);

      free(mod->items);
      mod->items = newList;
      mod->itemCapacity = newCap;
    }
  }

  // Add newly created item
  mod->items[mod->itemCount] = item;
  mod->itemCount++;
  // We do not support deleting items

  return 0;
}

int consumeElement(xmlTextReader * reader)
{
  return 0;
  // We do not support this element currently, so move the reader past it
}

channelInfo * parseWithReader(xmlTextReader * reader)
{
  channelInfo * channel = malloc(sizeof(channelInfo));
  memset(channel, 0, sizeof(channelInfo));

  bool skipNext = false;
  while (skipNext ? xmlTextReaderNext(reader) > 0 : xmlTextReaderRead(reader) > 0)
  {
    skipNext = false;
    xmlChar * name;
    xmlChar * ns;
    switch (xmlTextReaderNodeType(reader))
    {
      case XML_READER_TYPE_ELEMENT:
        name = xmlTextReaderName(reader);
        ns = xmlTextReaderPrefix(reader);
        if (strcmp(name, "item") == 0)
        {
          parseItemSubElement(reader, channel);
        }
        else if (strcmp(name, "image") == 0)
        {
          skipNext = true;
        }
        else if (ns && strcmp(ns, "podcast") == 0)
        {
          skipNext = true;
        }
        else
        {
          parseChannelSubElement(reader, channel);
        }
        free(name);
        if (ns)
          free(ns);
        break;
    }
  }
  return channel;
}

channelInfo * channelParseFd(int fd)
{
  writelog_l(LOGLEVEL_DEBUG, "parsing channel from fd %d", fd);
  xmlTextReader* reader = xmlReaderForFd(fd, NULL, NULL, 0);
  channelInfo * ret = parseWithReader(reader);
  xmlFreeTextReader(reader);
  return ret;
}

channelInfo * channelParseFname(char * fname)
{
  writelog_l(LOGLEVEL_DEBUG, "parsing channel from file %s", fname);
  xmlTextReader * reader = xmlReaderForFile(fname, NULL, 0);
  channelInfo * ret = parseWithReader(reader);
  xmlFreeTextReader(reader);
  return ret;
}
