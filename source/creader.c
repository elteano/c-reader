// Let's get an RSS reader working with this stuff

#define _GNU_SOURCE

#include<errno.h>
#include<locale.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<stdint.h>

#include<curl/curl.h>

#include<notcurses/notcurses.h>

#include<libxml/xmlreader.h>
#include<libxml/tree.h>

#include<sys/mman.h>

#include "parse.h"

#define TOP_BORDER  "\u2501"
#define SIDE_BORDER "\u2503"
#define TL_CORNER   "\u250f"
#define TR_CORNER   "\u2513"
#define BL_CORNER   "\u2517"
#define BR_CORNER   "\u251b"

#define ERR_NO_MEM  5

//#define DEBUG

// Feed:
// - Articles
// - Feed Name
// - Feed Logo?
// - ...?

// Article
// - Name
// - URL
// - Article Logo?

// Where is data stored?
// For now, just save the XML files and display based on parsings of those.

struct __channelList {
  channelInfo * channel;
  struct __channelList * prev;
  struct __channelList * next;
};

typedef struct _channelList {
  struct __channelList * root;
  struct __channelList * end;
} channelList;

int addChannel(channelList * list, channelInfo * channel)
{
  struct __channelList * nc = malloc(sizeof(struct __channelList));
  nc->channel = channel;
  nc->next = NULL;
  if (list->root)
  {
    nc->prev = list->end->prev;
    list->end->next = nc;
    list->end = nc;
  }
  else
  {
    nc->prev = NULL;
    nc->next = NULL;
    list->root = nc;
    list->end = nc;
  }
  return 0;
}

int removeChannel(channelList * list, channelInfo * channel)
{
  struct __channelList * old, * iter;
  if (list->root)
  {
    int n_removed = 0;
    while (list->root->channel == channel && list->root != list->end)
    {
      old = list->root;
      list->root = list->root->next;
      free(old);
      ++n_removed;
    }
    while (list->end->channel == channel && list->root != list->end)
    {
      old = list->root;
      list->end = list->end->prev;
      free(old);
      ++n_removed;
    }
    if (list->root == list->end && list->root->channel == channel)
    {
      free(list->root);
      list->root = NULL;
      list->end = NULL;
    }
    iter = list->root;
    while (iter)
    {
      old = iter->next;
      if (iter->channel == channel)
      {
        iter->prev->next = iter->next;
        iter->next->prev = iter->prev;
        free(iter);
        ++n_removed;
      }
      iter = old;
    }
    return n_removed;
  }
  else
  {
    return 0;
  }
}

struct viewpane
{
  struct ncplane * feedplane;
  struct ncplane * artplane;
  struct ncplane * infoplane;
};

int getFeed(char * feedurl)
{
  if (!feedurl)
  {
    return 1;
  }

  CURL * curl = curl_easy_init();
  if(curl)
  {
    CURLcode result;

    FILE * tmp = tmpfile();
    curl_easy_setopt(curl, CURLOPT_URL, feedurl);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, tmp);
    result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if(!result)
    {
      rewind(tmp);

      // Using xmllib2 for parsing
      xmlTextReaderPtr reader;
      // TODO parse XML
      // Seems that reader wants to be initialized with a filename

      // temporary file is deleted automatically with close
      fclose(tmp);
    }
    else
    {
      fclose(tmp);
      return 1;
    }
  }
  else
  {
    return 1;
  }
  return 0;
}

int geturl(char* url, FILE * dest)
{
  if (url && dest)
  {
    // Assuming that URl is good
    CURL * curl = curl_easy_init();
    if (curl)
    {
      // Temporary file storing downloaded content; deleted upon fclose
      CURLcode result;

      curl_easy_setopt(curl, CURLOPT_URL, url);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*) dest);
      result = curl_easy_perform(curl);

      if (result == CURLE_OK)
      {
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code >= 400)
        {
          result = -4;
        }
      }
      else if (result == CURLE_URL_MALFORMAT)
      {
        char * sa = malloc(sizeof(char) * 1024);
        snprintf(sa, 1024, "Curl bad url: %s\n", url);
        fputs(sa, dest);
        free(sa);
      }
      else
      {
        char * sa = malloc(sizeof(char) * 1024);
        snprintf(sa, 1024, "Curl error received: %d\n", result);
        fputs(sa, dest);
        free(sa);
      }

      curl_easy_cleanup(curl);

      rewind(dest);
      return result;
    }
    else
    {
      return -2;
    }
  }
  else
  {
    return -1;
  }
}

channelInfo * parseFromUrl(char * url)
{
  int memfd = memfd_create("xml", 0);
  FILE * fptr = fdopen(memfd, "r+");
  channelInfo * channel = NULL;
  if (geturl(url, fptr))
  {
    rewind(fptr);
    lseek(memfd, 0, SEEK_SET);
    channel = channelParseFd(memfd);
  }
  close(memfd);
  return channel;
}

/*
 * Testing function which receives the input character and prints, until ENTER
 * is input
 */
void * print_inputs(void * arg)
{
  struct notcurses * nc_handle = arg;
  struct ncplane * std_plane;
  ncinput val;
  std_plane = notcurses_stdplane(nc_handle);
  do
  {
    notcurses_get_blocking(nc_handle, &val);
    if (val.evtype == NCTYPE_PRESS || val.evtype == NCTYPE_REPEAT)
    {
      // Only process the key down, not the key up
      ncplane_putchar(std_plane, val.id);
      notcurses_render(nc_handle);
    }
  }
  while(val.id != NCKEY_ENTER);
  return NULL;
}

void create_title(struct ncplane * plane, char * title)
{
  int width, height;
  char * flip;
  flip = malloc(1024);
  ncplane_dim_yx(plane, &height, &width);
  sprintf(flip, "Height: %4d ; Width: %4d", height, width);
  ncplane_putstr_yx(plane, 2, 0, flip);
  free(flip);
  ncplane_putstr_yx(plane, 0, 0, title);
  for (int i = 0; i < width; ++i)
  {
    ncplane_putstr_yx(plane, 1, i, "\u2501");
  }
}

int getInput(struct notcurses * nc_handle, char * dest, int len)
{
  unsigned width, height;
  uint32_t res;
  ncinput val;
  memset(&val, 0, sizeof(ncinput));

  struct ncplane * std_plane = notcurses_stddim_yx(nc_handle, &height, &width);
  ncplane_options * opts = malloc(sizeof(ncplane_options));
  memset(opts, 0, sizeof(ncplane_options));
  opts->y = height / 2 - 2;
  opts->x = width / 2 - 30;
  opts->name = "Input Popup";
  opts->rows = 4;
  opts->cols = 60;
  struct ncplane * popup_plane = ncplane_create(std_plane, opts);
  free(opts);

  int rows = 0;
  int cols = 0;
  ncplane_dim_yx(popup_plane, &rows, &cols);

  for (int c = 0; c < cols; ++c)
  {
    ncplane_putstr_yx(popup_plane, 0,       c, TOP_BORDER);
    ncplane_putstr_yx(popup_plane, rows-1,  c, TOP_BORDER);
  }
  for (int r = 0; r < rows; ++r)
  {
    ncplane_putstr_yx(popup_plane, r, 0,      SIDE_BORDER);
    ncplane_putstr_yx(popup_plane, r, cols-1, SIDE_BORDER);
  }
  ncplane_putstr_yx(popup_plane, 0,       0,      TL_CORNER);
  ncplane_putstr_yx(popup_plane, 0,       cols-1, TR_CORNER);
  ncplane_putstr_yx(popup_plane, rows-1,  0,      BL_CORNER);
  ncplane_putstr_yx(popup_plane, rows-1,  cols-1, BR_CORNER);
  ncplane_cursor_move_yx(popup_plane, 2, 2);
  ncplane_putchar(popup_plane, '>');
  notcurses_render(nc_handle);
  char * statusStr = malloc(sizeof(char) * 1024);

  int cInd = 0;

  int cont = 1;
  do
  {
    notcurses_get_blocking(nc_handle, &val);
    snprintf(statusStr, 1024, "Status code: %d, %d", val.evtype, cInd);
    ncplane_putstr_yx(std_plane, 3, 3, statusStr);
    if (val.evtype == NCTYPE_PRESS)
    {
      if (val.id == NCKEY_ENTER)
      {
        cont = 0;
      }
      else
      {
        if (val.id == NCKEY_BACKSPACE)
        {
          if (cInd > 0)
          {
            --cInd;
          }
        }
        else
        {
          if (dest && cInd < len)
          {
            if (val.utf8[0] >= ' ' && val.utf8[0] < 127)
            {
              dest[cInd] = val.utf8[0];
              ncplane_putchar(popup_plane, dest[cInd]);
              ++cInd;
            }
          }
          else
          {
            cont = 0;
          }
        }
      }
    }
    notcurses_render(nc_handle);
  }
  while (cont);

  free(statusStr);

  notcurses_get_blocking(nc_handle, &val);
  ncplane_destroy(popup_plane);

  return 0;
}

int main(int argc, char** argv)
{
  int error = 0;
  channelInfo ** channels = malloc(sizeof(channelInfo*) * 1024);
  int num_channels = 0;
  struct notcurses * nc_handle;
  struct ncplane * std_plane;
  ncinput val;

  // Set locale to appropriate environment locale ; notcurses requires this
  setlocale(LC_ALL, "");

  // Initialize NotCurses with defaults
  nc_handle = notcurses_init(NULL, NULL);
  if (nc_handle)
  {
    struct viewpane * viewpane;
    viewpane = calloc(1, sizeof(struct viewpane));
    int height, width;

    std_plane = notcurses_stddim_yx(nc_handle, &height, &width);

    // Generate sub planes
    ncplane_options * opts = malloc(sizeof(ncplane_options));;
    memset(opts, 0, sizeof(ncplane_options));
    opts->y = 0;
    opts->x = 0;
    opts->name = "Feed Plane";
    opts->rows = height;
    opts->cols = width/3;
    viewpane->feedplane = ncplane_create(std_plane, opts);

    opts->y = 0;
    opts->x = width/3;
    opts->name = "Article Plane";
    opts->rows = height;
    opts->cols = width/3;
    viewpane->artplane = ncplane_create(std_plane, opts);

    opts->y = 0;
    opts->x = width / 3 * 2;
    opts->name = "Info Plane";
    opts->rows = height;
    opts->cols = width/3;
    viewpane->infoplane = ncplane_create(std_plane, opts);
    free(opts);
    opts = NULL;

    create_title(viewpane->feedplane, "Feeds");
    create_title(viewpane->artplane, "Articles");
    create_title(viewpane->infoplane, "Info");

    for (int i = 0; i < height; ++i)
    {
      ncplane_putstr_yx(viewpane->feedplane, i, width/3-1, SIDE_BORDER);
      ncplane_putstr_yx(viewpane->artplane, i, width/3-1, SIDE_BORDER);
    }
    ncplane_putstr_yx(viewpane->feedplane, 1, width/3-1, "\u254b");
    ncplane_putstr_yx(viewpane->artplane, 1, width/3-1, "\u254b");

    notcurses_render(nc_handle);

    // For now, we put everything on the UI thread.
    char * stuff    = malloc(sizeof(char) * 1024);
    char * getinfo  = malloc(sizeof(char) * 1024);
    if (stuff && getinfo)
    {
      do
      {
        memset(stuff,   0, 1024);
        memset(getinfo, 0, 1024);

        notcurses_get_blocking(nc_handle, &val);
        if (val.evtype == NCTYPE_PRESS && val.id == 'a')
        {
          getInput(nc_handle, getinfo, 1024);
          ncplane_putstr_yx(viewpane->feedplane, 30, 1, getinfo);
          notcurses_render(nc_handle);
          channelInfo * newChannel = parseFromUrl(getinfo);
          if(newChannel)
          {
            snprintf(stuff, 1024, "Channel title: %s", newChannel->title);
            ncplane_putstr_yx(viewpane->feedplane, 10, 10, stuff);
            channelFreeInfo(newChannel);
          }
          else
          {
            ncplane_putstr_yx(viewpane->feedplane, 10, 10, "Error.");
          }
          notcurses_render(nc_handle);
        }
      }
      while (val.id != 'q');
      free(stuff);
      free(getinfo);
      stuff   = NULL;
      getinfo = NULL;
    }
    else
    {
      error = ERR_NO_MEM;
    }

    // Close NotCurses
    // we don't care to destroy the planes because notcurses will take them with it
    free(viewpane);
    notcurses_stop(nc_handle);
    if (error)
    {
      switch (error)
      {
        case ERR_NO_MEM:
          fprintf(stderr, "Ran out of memory.\n");
          break;
      }
    }
  }
  else
  {
    fprintf(stderr, "%s\n", "Error initializing notcurses.");
  }
  free(channels);
}
