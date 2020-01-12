/*
 * wifi_storage.c
 *
 *  Created on: Dec 18, 2019
 *      Author: vvasilev
 */




/* Flash encryption Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/efuse_reg.h"
#include "esp_efuse.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_partition.h"
#include "esp_flash_encrypt.h"
#include "esp_efuse_table.h"
#include "nvs_flash.h"
#include "nvs.h"

static void example_print_chip_info(void);
static void example_print_flash_encryption_status(void);

#if ENBL_FL_W
static void example_read_write_flash(void);
#endif

static const char* TAG = "app_helper";

 void nvs_dump(const char *partName);



 void nvs_dump_partition(const char *partName, const char *namespace) {

	nvs_handle_t handle;
	nvs_stats_t nvs_stats;
	esp_err_t rc = nvs_get_stats(partName, &nvs_stats);
	nvs_iterator_t it;

	if(rc == ESP_OK) {
		//TODO: Dump stat
	}

	ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_open_from_partition(partName, namespace, NVS_READONLY, &handle));

	it = nvs_entry_find(partName, namespace, NVS_TYPE_ANY);

	typedef union {
		int8_t   int8;
		uint8_t  uint8;
		int16_t  int16;
		uint16_t uint16;
		int32_t  int32;
		uint32_t uint32;
		int64_t  int64;
		uint64_t uint64;
		char     data[100];
	} nvs_type_t;

	nvs_type_t data;

	while (it != NULL) {
	        nvs_entry_info_t info;
	        nvs_entry_info(it, &info);

	        switch(info.type) {
				case NVS_TYPE_U8:
					ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_get_u8  (handle, info.key,  &data.uint8));
					printf("key '%s', type '%d' value: %uc\n", info.key, info.type,data.uint8);
					break;
				case NVS_TYPE_I8:
					ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_get_i8  (handle, info.key,  &data.int8));
					printf("key '%s', type '%d' value: %c\n", info.key, info.type, data.int8);
					break;
				case NVS_TYPE_U16:
					ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_get_u16 (handle, info.key,  &data.uint16));
					printf("key '%s', type '%d' value: %u\n", info.key, info.type, data.uint16);
					break;
				case NVS_TYPE_I16:
					ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_get_i16 (handle, info.key,  &data.int16));
					printf("key '%s', type '%d' value: %d\n", info.key, info.type, data.int16);
					break;
				case NVS_TYPE_U32:
					ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_get_u32 (handle, info.key,  &data.uint32));
					printf("key '%s', type '%d' value: %u\n", info.key, info.type, data.uint32);
					break;
				case NVS_TYPE_I32:
					ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_get_i32 (handle, info.key,  &data.int32));
					printf("key '%s', type '%d' value: %d\n", info.key, info.type, data.int32);
					break;
				case NVS_TYPE_U64:
					ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_get_u64 (handle, info.key,  &data.uint64));
					printf("key '%s', type '%d' value: %llu\n", info.key, info.type, data.uint64);
					break;
				case NVS_TYPE_I64:
					ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_get_i64 (handle, info.key,  &data.int64));
					printf("key '%s', type '%d' value: %lld\n", info.key, info.type, data.int64);
					break;
				case NVS_TYPE_STR: {
			        size_t length = sizeof(data.data) - 1;
			        ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_get_str (handle, info.key,  data.data, &length));
			        printf("key '%s', type '%d' value: %s\n", info.key, info.type, data.data);
					break;
				}
				case NVS_TYPE_BLOB: {
			        size_t length = sizeof(data.data);
			        ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_get_blob (handle, info.key, data.data, &length));
			        data.data[length-1] = '\0';
			        printf("key '%s', type '%d' value: %s\n", info.key, info.type, data.data);
					break;
				}
				default:
					break;
	        }
	        it = nvs_entry_next(it);
	};

	nvs_release_iterator(it);
	nvs_close(handle);
}

void test_flash_init() {

	nvs_sec_cfg_t nvs_sec_cfg;
	esp_partition_iterator_t iter = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_PHY, "phy_init");

	esp_err_t rc = nvs_flash_read_security_cfg(esp_partition_get(iter), &nvs_sec_cfg);

	if ( rc != ESP_OK) {

	    ESP_LOGI(TAG, "Can't read NVS security config error code %s...", esp_err_to_name(rc));

		ESP_LOGI(TAG, "Try to generate NVS security config in partition %s", esp_partition_get(iter)->label);
		rc = nvs_flash_generate_keys(esp_partition_get(iter), &nvs_sec_cfg);
		ESP_ERROR_CHECK( rc );
	}
	else {
		ESP_LOGI(TAG, "Dump NVS Flash sec keys size %d", NVS_KEY_SIZE);
		ESP_LOG_BUFFER_HEXDUMP( TAG, nvs_sec_cfg.eky, NVS_KEY_SIZE, ESP_LOG_INFO );
		ESP_LOG_BUFFER_HEXDUMP( TAG, nvs_sec_cfg.tky, NVS_KEY_SIZE, ESP_LOG_INFO );
	}

	ESP_LOGI(TAG, "NVS Flash init");
    // Initialize NVS
    rc = nvs_flash_secure_init(&nvs_sec_cfg);//nvs_flash_init();
    if (rc == ESP_ERR_NVS_NO_FREE_PAGES || rc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
    	ESP_LOGI(TAG, "NVS Flash erase");
        ESP_ERROR_CHECK(nvs_flash_erase());
        rc = nvs_flash_secure_init(&nvs_sec_cfg);//nvs_flash_init();
    }
    ESP_ERROR_CHECK( rc );

    //nvs_dump("nvs");
    nvs_dump_partition("nvs", "test_nvs");
}

void dump_nvs_partition(const char* name){

	nvs_handle_t out_handle;

	ESP_ERROR_CHECK(nvs_open(NULL, NVS_READONLY, &out_handle));

}

void print_app_info(void)
{
    ESP_LOGI(TAG, "Example to check Flash Encryption status");

    example_print_chip_info();
    example_print_flash_encryption_status();
#if ENBL_FL_W
    example_read_write_flash();
#endif
}


static void example_print_chip_info(void)
{
    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    ESP_LOGI(TAG, "silicon revision %d, ", chip_info.revision);

    ESP_LOGI(TAG, "%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
}


static void example_print_flash_encryption_status(void)
{
    uint32_t flash_crypt_cnt = 0;
    esp_efuse_read_field_blob(ESP_EFUSE_FLASH_CRYPT_CNT, &flash_crypt_cnt, 7);
    ESP_LOGI(TAG, "FLASH_CRYPT_CNT eFuse value is %d\n", flash_crypt_cnt);

    esp_flash_enc_mode_t mode = esp_get_flash_encryption_mode();
    if (mode == ESP_FLASH_ENC_MODE_DISABLED) {
        ESP_LOGI(TAG, "Flash encryption feature is disabled\n");
    } else {
        ESP_LOGI(TAG, "Flash encryption feature is enabled in %s mode\n",
            mode == ESP_FLASH_ENC_MODE_DEVELOPMENT ? "DEVELOPMENT" : "RELEASE");
    }
}

#if ENBL_FL_W
static void example_read_write_flash(void)
{
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "storage");
    assert(partition);

    ESP_LOGI(TAG, "Erasing partition \"%s\" (0x%x bytes)\n", partition->label, partition->size);

    ESP_ERROR_CHECK(esp_partition_erase_range(partition, 0, partition->size));

    /* Generate the data which will be written */
    const size_t data_size = 32;
    uint8_t plaintext_data[data_size];
    for (uint8_t i = 0; i < data_size; ++i) {
        plaintext_data[i] = i;
    }

    ESP_LOGI(TAG, "Writing data with esp_partition_write:\n");
    ESP_LOG_BUFFER_HEXDUMP(TAG, plaintext_data, data_size, ESP_LOG_INFO);
    ESP_ERROR_CHECK(esp_partition_write(partition, 0, plaintext_data, data_size));

    uint8_t read_data[data_size];
    ESP_LOGI(TAG, "Reading with esp_partition_read:\n");
    ESP_ERROR_CHECK(esp_partition_read(partition, 0, read_data, data_size));
    ESP_LOG_BUFFER_HEXDUMP(TAG, read_data, data_size, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Reading with spi_flash_read:\n");
    ESP_ERROR_CHECK(spi_flash_read(partition->address, read_data, data_size));
    ESP_LOG_BUFFER_HEXDUMP(TAG, read_data, data_size, ESP_LOG_INFO);
}
#endif
