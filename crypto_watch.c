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
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
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

char * find_string(struct Page *page, char needle_s[], char needle_e[]) {
    char *start = strstr(page->buf, needle_s);
    if (start == NULL) {
        return NULL;
    }
    start += strlen(needle_s);

    char *end = strstr(start, needle_e);
    if (end == NULL) {
        return NULL;
    }
    
    char *buf = malloc(end - start);
    if (buf == NULL) {
        err(EXIT_FAILURE, "out of memory");
    }
    char *str = buf;
    while (start < end) {
        *str++ = *start++;
    }
    *str = '\0';
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
    if (page.size == 0) {
        free(page.address);
        return NULL;
    }

    char *str = find_string(&page, "<script id=\"__NEXT_DATA__\" type=\"application/json\">", "</script>");
    if (str == NULL) {
        return NULL;
    }

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

// copy at most len bytes from src into dest and null-terminate the result
void strcopy(char *dest, char *src, size_t len) {
    for (size_t i=0; i < len && *src != '\0'; i++) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// usage: ./crypto_watch bitcoin ethereum polkadot
int main(int argc, char **argv)
{
    // parse cli args
    size_t coins_size = argc - 1;
    char coin_names[24][24];
    if (coins_size > 0) {
        for (int i=0; i < coins_size && i < 24; i++) {
            strcopy(coin_names[i], argv[i+1], 24);
        }
    } else {
        // default to showing BTC and ETH
        coins_size = 2;
        strcpy(coin_names[0], "bitcoin");
        strcpy(coin_names[1], "ethereum");
    }

    // fetch data for each given coin
    struct Coin **coins = malloc(sizeof (struct Coin) * coins_size);
    for (int i=0; i < coins_size; i++) {
        coins[i] = get_coin(coin_names[i]);
        if (coins[i] == NULL) {
            err(EXIT_FAILURE, "unable to get data for coin %s", coin_names[i]);
        }
    }

    // print header     
    for (int i=0; i < coins_size; i++) {
        printf("%s $%.2f", coins[i]->symbol, coins[i]->price);

        if (i+1 < coins_size) {
            printf("\t");
        } else {
            printf("\n");
        }
    }

    // print list (dropdown)
    printf("---\n");
    printf("%-8s\t%-11s\t%-3s\t%-9s\t%-3s | font=monospace \n", "Coin", "Price", "Chg", "Volume", "Chg");
    for (int i=0; i < coins_size; i++) {
        printf("%-8s\t$%9.2f\t%+3.0f%%\t$%6.0fM\t%+3.0f%% | font=monospace\n", coins[i]->symbol, coins[i]->price, coins[i]->price_change_24h, coins[i]->volume / 1000000, coins[i]->volume_change_24h);
    }
    printf("---\n");

    // print footer
    char time_str[20];
    time_t curtime = time (NULL);
    struct tm * loc_time = localtime (&curtime);
    strftime(time_str, 100, "%I:%M %p", loc_time);
    printf("Last updated at %s | refresh=true \n", time_str);

    // free allocated memory
    for (int i=0; i < coins_size; i++) {
       free(coins[i]);
    }
    free(coins);
    return 0;
}

