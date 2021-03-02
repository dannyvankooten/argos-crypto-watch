#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON/cJSON.h"
#include <err.h>

struct Page {
    char * buf;
    size_t size;
    char * address;
};

static size_t download(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct Page *mem = (struct Page *) userp;
 
  char *ptr = realloc(mem->buf, mem->size + realsize + 1);
  if(ptr == NULL) {
    /* out of memory! */ 
    err(EXIT_FAILURE, "out of memory");
  }
 
  mem->buf = ptr;
  memcpy(&(mem->buf[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->buf[mem->size] = 0;
  mem->address = ptr;
 
  return realsize;
}

struct Page download_html(char url[]) {
    CURL *curl;
    CURLcode res;

    struct Page chunk;
    chunk.buf = malloc(1);  /* will be grown as needed by the realloc above */ 
    chunk.size = 0;    /* no data at this point */ 
    chunk.address = chunk.buf; 

    curl = curl_easy_init();
    if (!curl)
    {
        printf("Error initialising curl\n");
        return chunk;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &chunk);
    curl_easy_setopt(curl, CURL_SOCKET_TIMEOUT, 10);
    #ifdef __APPLE__
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    #endif

    res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl);
    return chunk;        
}

/* find first occurence of needle in page buffer */
int find_in_page(struct Page *page, char needle[]) {
    int i;
    int len = strlen(needle);
    for (i=0; i < page->size; i++) {
         if (memcmp(needle, &page->buf[i], len) == 0) {
           return i;
        }
    }

    return -1;
}

char * find_string(struct Page *page, char needle_s[], char needle_e[]) {
    int i_start = find_in_page(page, needle_s) + strlen(needle_s);
    page->buf += i_start;
    page->size -= i_start;
    int i_end = find_in_page(page, needle_e) + i_start;
    page->buf -= i_start;
    page->size += i_start;    

    char * buf = malloc(1024*512);
    if (buf == NULL) {
        err(EXIT_FAILURE, "out of memory");
    }
    for (int i=i_start, j=0; i < i_end; i++) {
        buf[j++] = page->buf[i];
    }

    return buf;
}

struct Coin {
    char symbol[12];
    double price;
    double volume;
    double price_change_24h;
    double volume_change_24h;
};

struct Coin *get_coin(char symbol[]) {
    char url[100] = "https://coinmarketcap.com/currencies/";
    strcat(url, symbol);
    struct Page page = download_html(url);
    char * str = find_string(&page, "<script id=\"__NEXT_DATA__\" type=\"application/json\">", "</script>");
    free(page.address);
    cJSON *json = cJSON_Parse(str);
    free(str);

    struct Coin *c = malloc(sizeof *c);
    if (c == NULL) {
        err(EXIT_FAILURE, "out of memory");
    }

    // https://coinmarketcap.com/currencies/bitcoin/
    // __NEXT_DATA__.props.initialProps.pageProps.info.statistics
    cJSON *obj = json;
    obj = cJSON_GetObjectItemCaseSensitive(obj, "props");
    obj = cJSON_GetObjectItemCaseSensitive(obj, "initialProps");
    obj = cJSON_GetObjectItemCaseSensitive(obj, "pageProps");
    obj = cJSON_GetObjectItemCaseSensitive(obj, "info");
    strcpy(c->symbol, cJSON_GetObjectItemCaseSensitive(obj, "symbol")->valuestring);
    c->volume = cJSON_GetObjectItemCaseSensitive(obj, "volume")->valuedouble;
    c->volume_change_24h = cJSON_GetObjectItemCaseSensitive(obj, "volumeChangePercentage24h")->valuedouble;
    obj = cJSON_GetObjectItemCaseSensitive(obj, "statistics");
    c->price = cJSON_GetObjectItemCaseSensitive(obj, "price")->valuedouble;
    c->price_change_24h = cJSON_GetObjectItemCaseSensitive(obj, "priceChangePercentage24h")->valuedouble;
    cJSON_Delete(json);
    return c;
}

int main(void)
{
    #define COINS_SIZE 2
    struct Coin *coins[COINS_SIZE] = {
        get_coin("bitcoin"),
        get_coin("ethereum"),
    };

    // print header     
    for (int i=0; i < COINS_SIZE; i++) {
        printf("%s $%.2f", coins[i]->symbol, coins[i]->price);

        if (i+1 < COINS_SIZE) {
            printf("\t\t");
        } else {
            printf("\n");
        }
    }

    // print list (dropdown)
    printf("---\n");
    for (int i=0; i < COINS_SIZE; i++) {
        printf("%s\t\t$%10.2f (%+.0f%%)\t\t$%6.0fM (%+.0f%%)\n", coins[i]->symbol, coins[i]->price, coins[i]->price_change_24h, coins[i]->volume / 1000000, coins[i]->volume_change_24h);
    }
    printf("---\n");

    // print footer
    char time_str[20];
    time_t curtime = time (NULL);
    struct tm * loc_time = localtime (&curtime);
    strftime(time_str, 100, "%I:%M %p", loc_time);
    printf("Last updated at %s | refresh=true \n", time_str);

    // free allocated memory
    for (int i=0; i < COINS_SIZE; i++) {
       free(coins[i]);
    }
    return 0;
}

