// Let's get an RSS reader working with this stuff

#include<errno.h>
#include<locale.h>
#include<stdio.h>
#include<stdlib.h>

#include<curl/curl.h>

#include<notcurses/notcurses.h>

#define PERR(text) fprintf(stderr, "%s\n", text)

FILE * geturl(char* url)
{
  if (url)
  {
    // Assuming that URl is good
    CURL * curl = curl_easy_init();
    if (curl)
    {
      // Temporary file storing downloaded content; deleted upon fclose
      CURLcode result;
      FILE * tmp = tmpfile();

      curl_easy_setopt(curl, CURLOPT_URL, url);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, tmp);

      result = curl_easy_perform(curl);
      curl_easy_cleanup(curl);

      if (!result)
      {
        // Caller will need to call this anyway
        rewind(tmp);

        // Caller needs to close this when done... this is why C is not the best
        return tmp;
      }
      else
      {
        fprintf(stderr, "Curl error code received: %d.\n", result);
      }
    }
  }
  return NULL;
}

int main(int argc, char** argv)
{
  struct notcurses * nc_handle;
  struct ncplane * std_plane;
  ncinput val;

  // Set locale to appropriate environment locale ; notcurses requires this
  setlocale(LC_ALL, "");

  // Initialize NotCurses with defaults
  nc_handle = notcurses_init(NULL, NULL);
  if (nc_handle)
  {
    std_plane = notcurses_stdplane(nc_handle);
    ncplane_set_bg_rgb(std_plane, 0);
    ncplane_putstr_yx(std_plane, 5, 10, "Let's test some draw!");

    notcurses_render(nc_handle);
    // Wait for input
    // First wait is to "clear" the input as it seems to fire too quickly
    notcurses_get_blocking(nc_handle, &val);
    notcurses_get_blocking(nc_handle, &val);
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

    // Close NotCurses
    notcurses_stop(nc_handle);
    // Give some output on the character we received
    printf("Found char %c\n", val.id);
  }
  else
  {
    PERR("Error initializing notcurses.");
  }
}
