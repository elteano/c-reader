// Let's get an RSS reader working with this stuff

#include<errno.h>
#include<stdio.h>
#include<stdlib.h>

#include<curl/curl.h>

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
  CURL * curl = curl_easy_init();
  if (curl)
  {
    CURLcode result;
    FILE * tmp = tmpfile();
    curl_easy_setopt(curl, CURLOPT_URL, "https://dvorak.substack.com/feed");
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, tmp);
    result = curl_easy_perform(curl);
    printf("result is %d\n", result);

    // Now read back from the temporary file
    rewind(tmp);
    char * buf = malloc(BUFSIZ);
    while(!feof(tmp))
    {
      fread(buf, sizeof(*buf), BUFSIZ, tmp);
      printf("%s", buf);
    }
    putchar('\n');
    fflush(stdout);

    // Clean up operations
    free(buf);
    fclose(tmp);
    curl_easy_cleanup(curl);
  }
}
