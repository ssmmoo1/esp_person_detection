
#
# Main component of TF Micro project 'person_detection'.
#

idf_component_register(
    SRCS
        "detection_responder.cc"
        "image_provider.cc"
        "main_functions.cc"
        "model_settings.cc"
        "person_detect_model_data.cc"
        "app_camera_esp.c"
        "esp_cli.c"
        "networking.c"
        "image_processing.c"
        "main.cc"

        PRIV_REQUIRES console tflite-lib esp32-camera screen static_images spi_flash fb_gfx protocol_examples_common nvs_flash fatfs sdmmc driver
    INCLUDE_DIRS ".")
