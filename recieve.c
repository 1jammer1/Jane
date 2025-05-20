// recv_audio.c
#include <microhttpd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8888

struct RequestData {
    FILE *fp;
};

static enum MHD_Result answer_to_connection(void *cls,
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *version,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls)
{
    struct RequestData *data = *con_cls;

    if (strcmp(url, "/mic") != 0 || strcmp(method, "POST") != 0)
        return MHD_NO;

    if (!data) {
        data = malloc(sizeof(*data));
        if (!data) return MHD_NO;
        data->fp = fopen("received.wav", "wb");
        if (!data->fp) { free(data); return MHD_NO; }
        *con_cls = data;
        return MHD_YES;
    }

    if (*upload_data_size) {
        fwrite(upload_data, 1, *upload_data_size, data->fp);
        *upload_data_size = 0;
        return MHD_YES;
    }

    fclose(data->fp);
    free(data);
    *con_cls = NULL;

    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen("Received"), (void*)"Received", MHD_RESPMEM_PERSISTENT);
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

int main() {
    struct MHD_Daemon *daemon;
    daemon = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, PORT, NULL, NULL,
                              &answer_to_connection, NULL, MHD_OPTION_NOTIFY_COMPLETED, NULL, NULL, MHD_OPTION_END);
    if (!daemon) {
        fprintf(stderr, "Failed to start server on port %d\n", PORT);
        return 1;
    }

    printf("Receiver running on http://127.0.0.1:%d/mic\n", PORT);
    getchar();  // wait for user to press Enter
    MHD_stop_daemon(daemon);
    return 0;
}
