// send_audio.c
#include <curl/curl.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s file.wav\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "curl_easy_init() failed\n");
        return 1;
    }

    curl_mime *form = curl_mime_init(curl);
    curl_mimepart *field = curl_mime_addpart(form);
    curl_mime_name(field, "file");
    curl_mime_filedata(field, filename);

    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:8887/upload");
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

    curl_mime_free(form);
    curl_easy_cleanup(curl);
    return 0;
}
