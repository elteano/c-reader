typedef struct __enclosureInfo
{
  char * link;
  int length;
  char * type;
} enclosureInfo;

typedef struct __itemInfo
{
  char * title;
  char * link;
  char * description;
  // TODO add multiple-enclosure support
  enclosureInfo * enclosure;
} itemInfo;

void enclosureFreeInfo(enclosureInfo*);
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
