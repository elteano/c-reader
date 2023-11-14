// Let's get an RSS reader working with this stuff

#define _GNU_SOURCE

#include<errno.h>
#include<locale.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>

#include<curl/curl.h>

#include<notcurses/notcurses.h>

#include<libxml/xmlreader.h>
#include<libxml/tree.h>

#include<sys/mman.h>

#include "parse.h"

#define BORDER_CHAR "\u2503"

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
  if (url)
  {
    // Assuming that URl is good
    CURL * curl = curl_easy_init();
    if (curl)
    {
      // Temporary file storing downloaded content; deleted upon fclose
      CURLcode result;

      curl_easy_setopt(curl, CURLOPT_URL, url);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, dest);

      result = curl_easy_perform(curl);
      curl_easy_cleanup(curl);

      if (!result)
      {
        rewind(dest);

        return 0;
      }
      else
      {
        fprintf(stderr, "Curl error code received: %d.\n", result);
        return result;
      }
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
  geturl(url, fptr);

  channelInfo * channel = channelParseFd(memfd);
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

char * getInput(struct notcurses * nc_handle)
{
  int width, height;
  struct ncplane * std_plane = notcurses_stddim_yx(nc_handle, &height, &width);
  ncplane_options opts;
  opts.y = height / 2 - 4;
  opts.x = width / 2 - 30;
  opts.name = "Input Popup";
  opts.rows = 8;
  opts.cols = 60;
  struct ncplane * popup_plane = ncplane_create(std_plane, &opts);
}

int main(int argc, char** argv)
{
  channelInfo ** channels = NULL;
#ifdef DEBUG
  channelInfo * channel = channelParseFname("noagenda.xml");
  printf("channel title: %s\n", channel->title);
  for (int i = 0; i < channel->itemCount; ++i)
  {
    printf("item title: %s\n", channel->items[i]->title);
    printf("item link: %s\n", channel->items[i]->link);
  }

  channelFreeInfo(channel);
  free(channel);

#else
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
    ncplane_options opts;
    opts.y = 0;
    opts.x = 0;
    opts.name = "Feed Plane";
    opts.rows = height;
    opts.cols = width/3;
    viewpane->feedplane = ncplane_create(std_plane, &opts);

    opts.y = 0;
    opts.x = width/3;
    opts.name = "Article Plane";
    opts.rows = height;
    opts.cols = width/3;
    viewpane->artplane = ncplane_create(std_plane, &opts);

    opts.y = 0;
    opts.x = width / 3 * 2;
    opts.name = "Info Plane";
    opts.rows = height;
    opts.cols = width/3;
    viewpane->infoplane = ncplane_create(std_plane, &opts);

    create_title(viewpane->feedplane, "Feeds");
    create_title(viewpane->artplane, "Articles");
    create_title(viewpane->infoplane, "Info");

    for (int i = 0; i < height; ++i)
    {
      ncplane_putstr_yx(viewpane->feedplane, i, width/3-1, BORDER_CHAR);
      ncplane_putstr_yx(viewpane->artplane, i, width/3-1, BORDER_CHAR);
    }
    ncplane_putstr_yx(viewpane->feedplane, 1, width/3-1, "\u254b");
    ncplane_putstr_yx(viewpane->artplane, 1, width/3-1, "\u254b");

    notcurses_render(nc_handle);
    // Wait for input
    // First wait is to "clear" the input as it seems to fire too quickly

    // For now, we put everything on the UI thread.
    do
    {
      notcurses_get_blocking(nc_handle, &val);
      if (val.evtype == NCTYPE_PRESS && val.id == 'a')
      {
        ncplane_putstr_yx(viewpane->feedplane, 1, 1, "Should get stream.");
        notcurses_render(nc_handle);
      }
    }
    while (val.id != 'q');

    //notcurses_get_blocking(nc_handle, &val);
    //notcurses_get_blocking(nc_handle, &val);

    /*
       pthread_t child;
       pthread_create(&child, NULL, &print_inputs, nc_handle);
       pthread_join(child, NULL);
       */

    // Close NotCurses
    // we don't care to destroy the planes because notcurses will take them with it
    free(viewpane);
    notcurses_stop(nc_handle);
  }
  else
  {
    fprintf(stderr, "%s\n", "Error initializing notcurses.");
  }
#endif
}
