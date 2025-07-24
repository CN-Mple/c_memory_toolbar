#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include "cJSON.h"

#define MAX_LINE_LENGTH (1024)
#define TOOL_BAR_WIDTH  (50)

char *INPUT_FILE = NULL;
char *FILE_CONFIG_JSON = NULL;

static char *search_keyword(const char *filename, const char *keyword);
static uint32_t parse_value(const char *line, const char *key, const char *format);
static char *fetch_config_json_string(void);
static void parse_execution_region(cJSON *region);
static void iterate_json(cJSON *root);

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <config_json_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE_CONFIG_JSON = argv[1];

    char *json_string = fetch_config_json_string();
    if (!json_string) {
        return EXIT_FAILURE;
    }

    cJSON *root = cJSON_Parse(json_string);
    free(json_string);

    if (!root) {
        fprintf(stderr, "Error parsing JSON: [%s]\n", cJSON_GetErrorPtr());
        return EXIT_FAILURE;
    }

    cJSON *project_file_name = cJSON_GetObjectItem(root, "target file");
    if (cJSON_IsString(project_file_name))
        INPUT_FILE = project_file_name->valuestring;

    // 调试用：打印完整JSON（可选）
    // char *str = cJSON_Print(root);
    // if (str) {
    //     printf("%s\n", str);
    //     free(str);
    // }

    iterate_json(root);

    cJSON_Delete(root);
    return EXIT_SUCCESS;
}

static char *search_keyword(const char *filename, const char *keyword)
{
    if (!filename || !keyword) {
        return NULL;
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error opening %s: %s\n", filename, strerror(errno));
        return NULL;
    }

    char line[MAX_LINE_LENGTH];
    char *result = NULL;

    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, keyword)) {
            size_t len = strlen(line);
            result = malloc(len + 1);
            if (result) {
                memcpy(result, line, len + 1);
            }
            break;
        }
    }

    fclose(file);
    return result;
}

static uint32_t parse_value(const char *line, const char *key, const char *format)
{
    if (!line || !key || !format) {
        return 0;
    }

    const char *ptr = strstr(line, key);
    if (!ptr) {
        return 0;
    }

    uint32_t value = 0;
    // 处理十六进制格式（如0x%x）
    if (strstr(format, "%x")) {
        if (sscanf(ptr, format, &value) != 1) {
            fprintf(stderr, "Failed to parse hex value for key: %s\n", key);
            return 0;
        }
    }
    // 处理十进制格式
    else if (strstr(format, "%d")) {
        int temp;
        if (sscanf(ptr, format, &temp) != 1) {
            fprintf(stderr, "Failed to parse decimal value for key: %s\n", key);
            return 0;
        }
        value = (uint32_t)temp;
    }

    return value;
}

static char *fetch_config_json_string(void)
{
    FILE *file = fopen(FILE_CONFIG_JSON, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open %s: %s\n",
                FILE_CONFIG_JSON, strerror(errno));
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (length <= 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = malloc(length + 1);
    if (!buffer) {
        fclose(file);
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    size_t read = fread(buffer, 1, length, file);
    buffer[read] = '\0';

    fclose(file);
    return buffer;
}

static void parse_execution_region(cJSON *region)
{
    if (!cJSON_IsObject(region)) {
        return;
    }

    const char *region_name = region->string;
    if (!region_name) {
        return;
    }

    char *line = search_keyword(INPUT_FILE, region_name);
    if (!line) {
        fprintf(stderr, "Region '%s' not found in map file\n", region_name);
        return;
    }

    cJSON *attribute = NULL;

    uint32_t toolbar[2];
    uint32_t count = 0;

    cJSON_ArrayForEach(attribute, region) {
        if (cJSON_IsString(attribute)) {
            uint32_t value = parse_value(line,
                                       attribute->string,
                                       attribute->valuestring);
            // printf("Region: %-25s %-15s = 0x%08X\n",
            //       region_name,
            //       attribute->string,
            //       value);
            if(count < 2) {
                toolbar[count++] = value;
            }
        }
    }
    float capacity = TOOL_BAR_WIDTH;
    float pecent = (float)toolbar[0] / (float)toolbar[1];
    float total_KB = toolbar[1] / 1024;
    printf("%s: \r\n", region->string);
    printf("%6.2fKB :|%*s%*s|    %6.2f%% (%6.2fKB / %6.2fKB) [%8dB / %8dB]\r\n", \
        total_KB, \
        (uint32_t)(capacity * pecent), "|", (uint32_t)(capacity * (1.0 - pecent)), " ", \
        100.0 * pecent, (float)toolbar[0] / 1024, (float)toolbar[1] / 1024, \
        toolbar[0], toolbar[1]);

    free(line);
}

static void iterate_json(cJSON *root)
{
    if (!cJSON_IsObject(root)) {
        fprintf(stderr, "Invalid JSON root structure\n");
        return;
    }

    cJSON *region = NULL;
    cJSON_ArrayForEach(region, root) {
        parse_execution_region(region);
    }
}
