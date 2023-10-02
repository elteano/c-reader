// Let's get an RSS reader working with this stuff

#include<errno.h>
#include<stdio.h>
#include<stdlib.h>

#include<curl/curl.h>

int test_main(int argc, char** argv)
{
  FILE * tmpf =  tmpfile();
  if (errno)
  {
    fprintf(stderr, "Errno set: %d\n", errno);
    return 1;
  }
  if (!tmpf)
  {
    fprintf(stderr, "Unable to get temporary file pointer: %x\n", tmpf);
    return 1;
  }

  // Ensure we can print to stdout
  printf("%s\n", "Testing output");

  // Write something to the temporary file
  fprintf(tmpf, "%s", "Test file write");
  fflush(tmpf); // Ensure that we get the data out
  rewind(tmpf); // Important to rewind file pointer back to beginning

  // Try to read from the temporary file
  char* buf = malloc(1024); // 1024 dseems a safe number
  int chars = fread(buf, sizeof(*buf), 1024, tmpf);
  printf("Read %d characters.\n", chars);
  printf("%s\n", buf);

  printf("%s\n", "Done writing.");
  // Release the resources
  free(buf);
  fclose(tmpf);
}

int main(int argc, char** argv)
{
  test_main(argc, argv);

  CURL * curl = curl_easy_init();
  if (curl)
  {
    printf("Successfully initialized curl.\n");
    CURLcode result;
    curl_easy_setopt(curl, CURLOPT_URL, "https://dvorak.substack.com/feed");
    result = curl_easy_perform(curl);
    printf("result is %d\n", result);
    curl_easy_cleanup(curl);
  }
}
