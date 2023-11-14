typedef struct __itemInfo
{
  char * title;
  char * link;
  char * description;
} itemInfo;

void itemFreeInfo(itemInfo*);

typedef struct __channelInfo
{
  char * title;
  char * link;
  char * description;
  // TODO better storage of items in struct
  itemInfo ** items;
  int itemCount;
  int itemCapacity;
} channelInfo;

void channelFreeInfo(channelInfo*);

channelInfo * channelParseFd(int);
channelInfo * channelParseFname(char *);
