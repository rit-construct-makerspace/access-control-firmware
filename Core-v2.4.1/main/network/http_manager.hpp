#include "esp_err.h"
namespace HTTPManager {
    // Init HTTP to one server in particular
    // (we're not made of memory)
    void init();
    enum class OperationType{
        GET,
        // POST,
    };
    // if this doesnt return ESP_OK, the operation will be cancelled
    // lifetime of memory at url_to_fill must be maintained until finish_cb is called
    using start_cb_t = esp_err_t(*)(void *user_data, const char **url_to_fill);

    // called when new data has come from the network on GET or when we need data to upload POST
    // when downloading, *len is length of data
    // when uploading set *len to length of data
    using data_cb_t = esp_err_t (*)(void *user_data, uint8_t* data, size_t* len);

    // if err != ESP_OK something bad happened, called on good or bad finish
    using finish_cb_t = void(*)(void *user_data, esp_err_t err);

    struct Transfer{
        OperationType type;
        start_cb_t start;
        data_cb_t data;
        finish_cb_t finish;
        void *user_data;
    };

    bool queue_transfer(Transfer xfer);
} // namespace HTTPManager