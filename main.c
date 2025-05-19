#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <microhttpd.h>
#include <curl/curl.h>
#include <getopt.h>

#define UPLOAD_DIR "uploads"
#define URL_DEFAULT "http://localhost:8887/upload"

/* --------------------- HTTP Server (Upload Handler) --------------------- */

#define POSTBUFFERSIZE  512
#define MAX_FILENAME    256
#define MAX_POST_SIZE   (50 * 1024 * 1024) // 50 MB

struct connection_info_struct {
    char *buff;
    size_t size;
    FILE *fp;
    char filename[MAX_FILENAME];
    int post_processed;
};

static int
save_uploaded(const char *upload_data, size_t upload_data_size,
              struct connection_info_struct *con_info) {
    if (!con_info->fp) {
        return MHD_NO;
    }
    fwrite(upload_data, 1, upload_data_size, con_info->fp);
    return MHD_YES;
}

static int
iterate_post(void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
             const char *filename, const char *content_type,
             const char *transfer_encoding, const char *data, uint64_t off, size_t size) {
    struct connection_info_struct *con_info = coninfo_cls;
    if (0 == strcmp(key, "file") && filename && size > 0) {
        /* sanitize filename: remove path */
        const char *clean = strrchr(filename, '/');
        clean = clean ? clean + 1 : filename;
        snprintf(con_info->filename, MAX_FILENAME, "%s/%s", UPLOAD_DIR, clean);
        con_info->fp = fopen(con_info->filename, "wb");
        if (!con_info->fp) return MHD_NO;
        con_info->post_processed = 1;
    }
    if (con_info->post_processed) {
        save_uploaded(data, size, con_info);
    }
    return MHD_YES;
}

static void
request_completed(void *cls, struct MHD_Connection *connection,
                  void **con_cls, enum MHD_RequestTerminationCode toe) {
    struct connection_info_struct *con_info = *con_cls;
    if (NULL == con_info) return;
    if (con_info->fp) fclose(con_info->fp);
    free(con_info->buff);
    free(con_info);
    *con_cls = NULL;
}

static int
answer_to_connection(void *cls, struct MHD_Connection *connection,
                     const char *url, const char *method,
                     const char *version, const char *upload_data,
                     size_t *upload_data_size, void **con_cls) {
    if (0 != strcmp(method, "POST")) return MHD_NO;
    if (NULL == *con_cls) {
        struct connection_info_struct *con_info;
        con_info = calloc(1, sizeof(*con_info));
        if (!con_info) return MHD_NO;
        con_info->buff = malloc(POSTBUFFERSIZE);
        con_info->size = 0;
        *con_cls = con_info;
        return MHD_YES;
    }
    struct connection_info_struct *con_info = *con_cls;
    if (*upload_data_size != 0) {
        MHD_post_process(con_info->postprocessor, upload_data, *upload_data_size);
        *upload_data_size = 0;
        return MHD_YES;
    } else {
        /* Send response JSON */
        const char *json = "{ \"message\": \"File uploaded successfully\" }";
        struct MHD_Response *response = MHD_create_response_from_buffer(strlen(json), (void*)json, MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Content-Type", "application/json");
        int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }
}

static struct MHD_Daemon *daemon;

/* --------------------- HTTP Client (Send Audio) --------------------- */

int send_audio(const char *file_path, const char *url) {
    CURL *curl;
    CURLcode res;
    FILE *fp = fopen(file_path, "rb");
    struct stat file_info;

    if (!fp) {
        fprintf(stderr, "Failed to open file: %s\n", file_path);
        return 1;
    }
    if (stat(file_path, &file_info) != 0) {
        fprintf(stderr, "Could not get file info: %s\n", file_path);
        fclose(fp);
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        return 1;
    }

    /* Set URL, headers and read callback */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READDATA, fp);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_info.st_size);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    /* Cleanup */
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    fclose(fp);
    return (res == CURLE_OK) ? 0 : 1;
}

/* --------------------- Main and CLI --------------------- */

void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --server <port> | --client <file_path> [--url <url>]\n",
            prog);
}

int main(int argc, char *argv[]) {
    int opt;
    int option_index = 0;
    int server_mode = 0;
    int client_mode = 0;
    char *file_path = NULL;
    char *url = NULL;
    int port = 0;

    static struct option long_options[] = {
        {"server", required_argument, 0, 's'},
        {"client", required_argument, 0, 'c'},
        {"url",    required_argument, 0, 'u'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "s:c:u:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 's':
                server_mode = 1;
                port = atoi(optarg);
                break;
            case 'c':
                client_mode = 1;
                file_path = optarg;
                break;
            case 'u':
                url = optarg;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (server_mode) {
        /* Ensure upload directory */
        mkdir(UPLOAD_DIR, 0755);

        /* Start HTTP server */
        daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY | MHD_USE_POLL,
                                  port, NULL, NULL,
                                  &answer_to_connection, NULL,
                                  MHD_OPTION_NOTIFY_COMPLETED, request_completed, NULL,
                                  MHD_OPTION_END);
        if (!daemon) {
            fprintf(stderr, "Failed to start server\n");
            return 1;
        }
        printf("Server listening on port %d...\n", port);
        getchar(); /* Wait for any key to exit */
        MHD_stop_daemon(daemon);
        return 0;
    } else if (client_mode) {
        url = url ? url : URL_DEFAULT;
        return send_audio(file_path, url);
    } else {
        print_usage(argv[0]);
        return 1;
    }
}

