// Let's get an RSS reader working with this stuff

#define _GNU_SOURCE

#include<stdarg.h>
#include<errno.h>
#include<locale.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<stdint.h>
#include<time.h>

#include<curl/curl.h>

#include<notcurses/notcurses.h>

#include<libxml/xmlreader.h>
#include<libxml/tree.h>
#include<libxml/xmlstring.h>

#include<sys/mman.h>
#include<sys/param.h>

#include "std.h"
#include "parse.h"
#include "logging.h"
#include "notcutils.h"

#define ERR_NO_MEM  5

#define DEBUG

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

#ifdef DEBUG
void fcpy(FILE * dest, FILE *src)
{
  char * buf = malloc(BUFSIZ);
  writelog_l(LOGLEVEL_DEBUG, "performing fcpy with bufsiz %d", BUFSIZ);

  while (fgets(buf, BUFSIZ, src))
  {
    fputs(buf, dest);
  }
  fflush(dest);

  free(buf);
  buf = NULL;
}
#endif

enum cursor_pos_t
{
  POS_CHANNEL,
  POS_ITEM
};

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
  struct ncplane * headerplane;
  struct ncplane * feedplane;
  struct ncplane * artplane;
  struct ncplane * infoplane;
};

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
          writelog_l(LOGLEVEL_ERROR, "curl bad response code: %ld", response_code);
          result = -4;
        }
        else
        {
          writelog_l(LOGLEVEL_INFO, "curl response code: %ld", response_code);
        }
      }
      else if (result == CURLE_URL_MALFORMAT)
      {
        writelog_l(LOGLEVEL_ERROR, "curl bad url: %s", url);
      }
      else
      {
        writelog_l(LOGLEVEL_ERROR, "curl error received: %d", result);
      }

      curl_easy_cleanup(curl);

      rewind(dest);
      return !(result == CURLE_OK);
    }
    else
    {
      writelog_l(LOGLEVEL_ERROR, "geturl: curl not initialized.");
      return -2;
    }
  }
  else
  {
    writelog_l(LOGLEVEL_WARN, "geturl: Null input received.");
    return -1;
  }
}

channelInfo * parseFromUrl(char * url)
{
  int memfd = memfd_create("xml", 0);
  FILE * fptr = fdopen(memfd, "r+");
  channelInfo * channel = NULL;
  if (!geturl(url, fptr))
  {
#ifdef DEBUG
    rewind(fptr);
    FILE * tmpf = fopen("debug-file.xml", "w");
    fcpy(tmpf, fptr);
    fclose(tmpf);
    tmpf = NULL;
#endif
    rewind(fptr);
    lseek(memfd, 0, SEEK_SET);
    channel = channelParseFd(memfd);
  }
  else
  {
    writelog_l(LOGLEVEL_ERROR, "parse error: geturl bad result");
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
  unsigned width, height;
  ncplane_dim_yx(plane, &height, &width);
  ncplane_putstr_yx(plane, 0, 0, title);
  for (int i = 0; i < width; ++i)
  {
    ncplane_putstr_yx(plane, 1, i, TOP_BORDER_CHAR);
  }
}

/*
 * Print channel title information onto a plane given a list and quantity of channels.
 *
 * plane: target for channel title writing
 * channls: list of pointers to channels with information
 * numChannels: number of channels in above list
 */
int printTitles(struct ncplane * plane, channelInfo ** channels, int numChannels)
{
  if (channels)
  {
    if (plane)
    {
      writelog_l(LOGLEVEL_TRACE, "printing %d channels to plane", numChannels);
      for (int i = 0; i < numChannels; ++i)
      {
        writelog_l(LOGLEVEL_DEBUG, "printing channel title %s", channels[i]->title);
        ncplane_putstr_yx(plane, i, 1, __CC(channels[i]->title));
      }
      return 0;
    }
    else
    {
      writelog_l(LOGLEVEL_WARN, "Tried printing, but plane was null.");
      return 1;
    }
  }
  else
  {
    writelog_l(LOGLEVEL_WARN, "Tried printing, but channels was null.");
    return 1;
  }
}

/*
 * Print items from the given channel upon the given plane. Most likely
 * createArticles should be used and a new plane created. This makes no attempt
 * to clear any previous drawings upon the plane.
 *
 * plane: Plane upon which to write item information
 * channel: source channel providing item information
 */
int printArticles(struct ncplane * plane, channelInfo * channel)
{
  writelog_l(LOGLEVEL_TRACE, "printing %d articles to plane", channel->itemCount);
  unsigned rows;
  unsigned cols;
  char * buf;

  ncplane_dim_yx(plane, &rows, &cols);
  buf = malloc(cols+4);

  writelog_l(LOGLEVEL_TRACE, "writing items from channel %s", channel->title);
  int avail_rows = MIN(rows, channel->itemCount);
  for (int i = 0; i < avail_rows; ++i)
  {
    if (strlen(__CC(channel->items[i]->title)) > cols-1)
    {
      memcpy(buf, channel->items[i]->title, cols-4);
      strcpy(&buf[cols-4], "\u2026");
    }
    else
    {
      strcpy(buf, __CC(channel->items[i]->title));
    }
    ncplane_putstr_yx(plane, i, 1, buf);
  }
  free(buf);
  return 0;
}

/*
 * Create a plane and list the items of the given channel.
 * base: Parent plane upon which the new plane is drawn
 * channel: source channel providing item information
 */
struct ncplane * createArticles(struct ncplane * base, channelInfo * channel)
{
  writelog_l(LOGLEVEL_TRACE, "creating new article plane for %s", channel->title);
  unsigned rows;
  unsigned cols;
  unsigned indiv_cols;
  struct ncplane * ret;
  struct ncplane_options opts;

  ncplane_dim_yx(base, &rows, &cols);
  indiv_cols = (cols - SIDE_PADDING*2 - INTER_PADDING*2)/3;

  memset(&opts, 0, sizeof(ncplane_options));
  opts.y = TOP_PADDING;
  opts.x = SIDE_PADDING + indiv_cols + INTER_PADDING;
  opts.name = "Article Plane";
  opts.rows = rows - TOP_PADDING;
  opts.cols = indiv_cols;

  ret = ncplane_create(base, &opts);
  printArticles(ret, channel);
  return ret;
}

/*
 * Creates a header plane displaying titles of the various regions
 * base: the parent panel upon which the header will be drawn
 */
struct ncplane * createHeader(struct ncplane * base)
{
  unsigned rows, cols;
  ncplane_dim_yx(base, &rows, &cols);

  // Set up the plane with the appropriate size
  ncplane_options opts;
  memset(&opts, 0, sizeof(ncplane_options));
  opts.y = 0;
  opts.x = 0;
  opts.name = "Header Plane";
  opts.rows = TOP_PADDING;
  opts.cols = cols;
  unsigned space = (cols - SIDE_PADDING * 2 - INTER_PADDING * 2)/3;

  struct ncplane * ret = ncplane_create(base, &opts);

  // Draw the headers
  ncplane_cursor_move_yx(ret, 0, 0);
  for (int i = 0; i < cols; ++i)
  {
    ncplane_putstr(ret, "\u259e");
  }
  ncplane_cursor_move_yx(ret, TOP_PADDING-1, 0);
  for (int i = 0; i < cols; ++i)
  {
    ncplane_putstr(ret, TOP_BORDER_CHAR);
  }
  for (int r = 1; r < TOP_PADDING-1; ++r)
  {
    ncplane_putstr_yx(ret, r, SIDE_PADDING+space+INTER_PADDING/2, SIDE_BORDER_CHAR);
    ncplane_putstr_yx(ret, r, SIDE_PADDING+space*2+INTER_PADDING, SIDE_BORDER_CHAR);
  }
  ncplane_putstr_yx(ret, TOP_PADDING-1, SIDE_PADDING+space+INTER_PADDING/2, CT_CORNER_CHAR);
  ncplane_putstr_yx(ret, TOP_PADDING-1, SIDE_PADDING+space*2+INTER_PADDING, CT_CORNER_CHAR);

  ncplane_putstr_yx(ret, TOP_PADDING-2, SIDE_PADDING, "Feeds");
  ncplane_putstr_yx(ret, TOP_PADDING-2, SIDE_PADDING+space+INTER_PADDING, "Articles");
  ncplane_putstr_yx(ret, TOP_PADDING-2, SIDE_PADDING+space*2+INTER_PADDING*2, "Info");

  return ret;
}

/*
 * Create the "info" plane on the right region of the provided panel.
 * Base: Parent panel on which info panel will be placed
 * item: Source material to be placed in the panel
 * nc_handle: if NULL, no attempt made to print image. if not NULL, we see buggy implementation of displaying image
 */
struct ncplane * createInfo(struct ncplane * base, itemInfo * item, struct notcurses * nc_handle)
{
  writelog_l(LOGLEVEL_TRACE, "creating new item plane for %s", item->title);
  unsigned rows;
  unsigned cols;
  unsigned indiv_cols;
  struct ncplane * ret;
  struct ncplane_options opts;

  ncplane_dim_yx(base, &rows, &cols);
  indiv_cols = (cols - SIDE_PADDING*2 - INTER_PADDING*2)/3;

  memset(&opts, 0, sizeof(ncplane_options));
  opts.y = TOP_PADDING;
  opts.x = SIDE_PADDING + indiv_cols*2 + INTER_PADDING*2;
  opts.name = "Item Plane";
  opts.rows = rows - TOP_PADDING;
  opts.cols = indiv_cols;

  ret = ncplane_create(base, &opts);
  size_t bytes = 0;
  if (nc_handle && item->enclosure && item->enclosure->link)
  {
    writelog_l(LOGLEVEL_TRACE, "getting info with image");
    // Download the file, have NotCurses read the file, calculate the location, and display
    FILE * tmpfile = fopen("/tmp/imgdata.tmp", "w+");
    geturl(__CC(item->enclosure->link), tmpfile);
    fflush(tmpfile);
    fclose(tmpfile);
    tmpfile = NULL;
    struct ncvisual * img = ncvisual_from_file("/tmp/imgdata.tmp");

    // Set the options to determine location and size
    struct ncvisual_options * ncv_opts = calloc(1, sizeof(struct ncvisual_options));
    // geom should be populated based on the options
    struct ncvgeom * ncv_geom = calloc(1, sizeof(struct ncvgeom));

    // base plane
    ncv_opts->n = ret;
    ncv_opts->scaling = NCSCALE_STRETCH;
    ncv_opts->y = 0;
    ncv_opts->x = 0;
    ncv_opts->begy = 0;
    ncv_opts->begx = 0;
    ncv_opts->leny = 20;
    ncv_opts->lenx = indiv_cols;
    ncv_opts->blitter = NCBLIT_3x2;
    ncv_opts->flags = NCVISUAL_OPTION_BLEND;
    ncv_opts->transcolor = 0;
    ncv_opts->pxoffy = 0;
    ncv_opts->pxoffx = 0;
    writelog_l(LOGLEVEL_TRACE, "opts set for render");

    /*
    ncvisual_geom(nc_handle, img, ncv_opts, ncv_geom);
    writelog_l(LOGLEVEL_DEBUG, "geom received: pixel size (%ux%u), cell (%ux%u); blitter %u", ncv_geom->pixx, ncv_geom->pixy, ncv_geom->rcellx, ncv_geom->rcelly, ncv_geom->blitter);
    */
    ncvisual_blit(nc_handle, img, ncv_opts);
    writelog_l(LOGLEVEL_TRACE, "blit called");
    if (item->description)
    {
      ncplane_puttext(ret, 20, NCALIGN_LEFT, __CC(item->description), &bytes);
      writelog_l(LOGLEVEL_TRACE, "puttext reports %u bytes", bytes);
    }
    else
    {
      writelog_l(LOGLEVEL_WARN, "no description for item %s", item->title);
    }
    //remove("/tmp/imgdata.tmp");

    free(ncv_opts);
    free(ncv_geom);
  }
  else
  {
    if (item->description)
    {
      ncplane_puttext(ret, 0, NCALIGN_LEFT, __CC(item->description), &bytes);
      writelog_l(LOGLEVEL_TRACE, "puttext reports %u bytes", bytes);
    }
    else
    {
      writelog_l(LOGLEVEL_WARN, "no description for item %s", item->title);
    }
  }
  return ret;
}

int main(int argc, char** argv)
{
  openlog();
  int error = 0;
  channelInfo ** channels = malloc(sizeof(channelInfo*) * 1024);
  int num_channels = 0;
  struct notcurses * nc_handle;
  struct ncplane * std_plane;
  ncinput val;
  enum cursor_pos_t cursor_pos = POS_CHANNEL;
  int channel_sel = 0;
  int item_sel = 0;

  // Set locale to appropriate environment locale ; notcurses requires this
  setlocale(LC_ALL, "");

  // Initialize NotCurses with defaults
  nc_handle = notcurses_init(NULL, NULL);
  if (nc_handle)
  {
    writelog("notcurses initialized");
    struct viewpane * viewpane;
    viewpane = calloc(1, sizeof(struct viewpane));
    unsigned height, width;

    std_plane = notcurses_stddim_yx(nc_handle, &height, &width);

    // Generate sub planes
    // The sub planes only list the data, so they will be tossed and recreated as needed
    unsigned indiv_cols = (width - SIDE_PADDING*2 - INTER_PADDING*2)/3;
    ncplane_options * opts = malloc(sizeof(ncplane_options));;
    memset(opts, 0, sizeof(ncplane_options));

    viewpane->headerplane = createHeader(std_plane);

    opts->y = TOP_PADDING;
    opts->x = SIDE_PADDING;
    opts->name = "Feed Plane";
    opts->rows = height - TOP_PADDING;
    opts->cols = indiv_cols;
    viewpane->feedplane = ncplane_create(std_plane, opts);

    //opts->y = TOP_PADDING;
    opts->x = indiv_cols + SIDE_PADDING + INTER_PADDING;
    opts->name = "Article Plane";
    //opts->rows = height - TOP_PADDING;
    //opts->cols = indiv_cols;
    viewpane->artplane = ncplane_create(std_plane, opts);

    //opts->y = TOP_PADDING;
    opts->x = indiv_cols*2 + SIDE_PADDING + INTER_PADDING*2;
    opts->name = "Info Plane";
    //opts->rows = height - TOP_PADDING;
    //opts->cols = indiv_cols;
    viewpane->infoplane = ncplane_create(std_plane, opts);

    free(opts);
    opts = NULL;

    /*
    create_title(viewpane->feedplane, "Feeds");
    create_title(viewpane->artplane, "Articles");
    create_title(viewpane->infoplane, "Info");
    */

    for (int i = 0; i < height; ++i)
    {
      ncplane_putstr_yx(viewpane->feedplane, i, width/3-1, SIDE_BORDER_CHAR);
      ncplane_putstr_yx(viewpane->artplane, i, width/3-1, SIDE_BORDER_CHAR);
    }
    ncplane_putstr_yx(viewpane->feedplane, 1, width/3-1, CT_CORNER_CHAR);
    ncplane_putstr_yx(viewpane->artplane, 1, width/3-1, CT_CORNER_CHAR);

    notcurses_render(nc_handle);

    // For now, we put everything on the UI thread.
    char * getinfo  = malloc(sizeof(char) * 1024);
    if (getinfo)
    {
      do
      {
        notcurses_get_blocking(nc_handle, &val);
        if (val.evtype == NCTYPE_PRESS)
        {
          if (val.id == 'a')
          {
            getInput(nc_handle, getinfo, 1024, "Feed URL");
            if (strlen(getinfo))
            {
              writelog("fetching url %s", getinfo);
              channelInfo * newChannel = parseFromUrl(getinfo);
              if(newChannel)
              {
                writelog("found channel with title %s", newChannel->title);
                channels[num_channels] = newChannel;
                ++num_channels;
                printTitles(viewpane->feedplane, channels, num_channels);

                ncplane_destroy(viewpane->artplane);
                viewpane->artplane = createArticles(std_plane, newChannel);
              }
              else
              {
                writelog_l(LOGLEVEL_ERROR, "unable to create channel for url %s", getinfo);
              }
              notcurses_render(nc_handle);
            }
            else
            {
              writelog("empty response from user, skipping fetch");
            }
            memset(getinfo, 0, 1024);
          }
          else if ((val.id == 'l' || val.id == 'L') && ncinput_ctrl_p(&val))
          {
            writelog_l(LOGLEVEL_DEBUG, "refreshing notcurses");
            //TODO resize the panels
            notcurses_refresh(nc_handle, NULL, NULL);
          }
          else if (num_channels > 0)
          {
            if (val.id == NCKEY_ENTER)
            {
              writelog_l(LOGLEVEL_DEBUG, "Want to download url from (%d, %d): %s", channel_sel, item_sel, channels[channel_sel]->items[item_sel]->link);
              ncplane_destroy(viewpane->infoplane);
              viewpane->infoplane = createInfo(std_plane, channels[channel_sel]->items[item_sel], nc_handle);
              notcurses_render(nc_handle);
            }
            else if (val.id == 'j')
            {
              if (cursor_pos == POS_CHANNEL)
              {
                if (channel_sel < num_channels-1)
                {
                  ncplane_putchar_yx(viewpane->feedplane, channel_sel, 0, ' ');
                  ++channel_sel;

                  ncplane_destroy(viewpane->artplane);
                  viewpane->artplane = createArticles(std_plane, channels[channel_sel]);

                  item_sel = 0;
                  ncplane_putstr_yx(viewpane->artplane, item_sel, 0, SILENT_ARROW_CHAR);
                }
                ncplane_putstr_yx(viewpane->feedplane, channel_sel, 0, ARROW_CHAR);
              }
              else if (cursor_pos == POS_ITEM)
              {
                if (item_sel < channels[channel_sel]->itemCount-1)
                {
                  ncplane_putchar_yx(viewpane->artplane, item_sel, 0, ' ');
                  ++item_sel;
                  ncplane_putstr_yx(viewpane->artplane, item_sel, 0, ARROW_CHAR);

                  ncplane_destroy(viewpane->infoplane);
                  viewpane->infoplane = createInfo(std_plane, channels[channel_sel]->items[item_sel], NULL);
                }
              }
            }
            else if (val.id == 'k')
            {
              if (cursor_pos == POS_CHANNEL)
              {
                if (channel_sel > 0)
                {
                  ncplane_putchar_yx(viewpane->feedplane, channel_sel, 0, ' ');
                  --channel_sel;

                  ncplane_destroy(viewpane->artplane);
                  viewpane->artplane = createArticles(std_plane, channels[channel_sel]);

                  item_sel = 0;
                  ncplane_putstr_yx(viewpane->artplane, item_sel, 0, SILENT_ARROW_CHAR);
                }
                ncplane_putstr_yx(viewpane->feedplane, channel_sel, 0, ARROW_CHAR);
              }
              else if (cursor_pos == POS_ITEM)
              {
                if (item_sel > 0)
                {
                  ncplane_putchar_yx(viewpane->artplane, item_sel, 0, ' ');
                  --item_sel;
                  ncplane_putstr_yx(viewpane->artplane, item_sel, 0, ARROW_CHAR);

                  ncplane_destroy(viewpane->infoplane);
                  viewpane->infoplane = createInfo(std_plane, channels[channel_sel]->items[item_sel], 0);
                }
              }
            }
            else if (val.id == 'l')
            {
              if (cursor_pos == POS_CHANNEL)
              {
                writelog_l(LOGLEVEL_DEBUG, "moving cursor to item plane");
                ncplane_putstr_yx(viewpane->feedplane, channel_sel, 0, SILENT_ARROW_CHAR);
                writelog_l(LOGLEVEL_DEBUG, "putting arrow on item plane");
                ncplane_putstr_yx(viewpane->artplane, item_sel, 0, ARROW_CHAR);
                writelog_l(LOGLEVEL_DEBUG, "done");
                cursor_pos = POS_ITEM;
              }
            }
            else if(val.id == 'h')
            {
              if (cursor_pos == POS_ITEM)
              {
                cursor_pos = POS_CHANNEL;
                ncplane_putstr_yx(viewpane->artplane, item_sel, 0, SILENT_ARROW_CHAR);
                ncplane_putstr_yx(viewpane->feedplane, channel_sel, 0, ARROW_CHAR);
              }
            }
          }
          notcurses_render(nc_handle);
        }
      }
      while (val.id != 'q');
      free(getinfo);
      getinfo = NULL;
    }
    else
    {
      error = ERR_NO_MEM;
      writelog("Ran out of memory.");
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
          writelog("Ran out of memory.");
          break;
      }
    }
  }
  else
  {
    writelog_l(LOGLEVEL_ERROR, "Error initializing notcurses.");
    fprintf(stderr, "%s\n", "Error initializing notcurses.");
  }
  for (int i = 0; i < num_channels; ++i)
  {
    channelFreeInfo(channels[i]);
  }
  free(channels);
  closelog();
}
