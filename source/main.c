#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <3ds.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MAX_FILES 256
#define MAX_FILENAME_LEN 256
#define TOP_SCREEN_ROWS 30
#define TOP_SCREEN_COLS 50
#define BOTTOM_SCREEN_ROWS 30
#define BOTTOM_SCREEN_COLS 40
#define REPEAT_START_DELAY 500
#define REPEAT_CONTINUE_DELAY 100

typedef struct {
    char name[MAX_FILENAME_LEN];
    bool is_dir;
} FileEntry;

static PrintConsole topScreen, bottomScreen;
static FileEntry file_list[MAX_FILES];
static int file_count = 0;
static int cursor = 0;
static int scroll_offset = 0;
static char current_path[1024] = "sdmc:/";

void populate_file_list();
void draw_ui();
bool handle_action_keys(u32 kDown);
void move_cursor(int direction);
bool convert_image(const char* input_path);
void show_message(const char* message, bool wait_for_key);
int compare_files(const void* a, const void* b);

int compare_files(const void* a, const void* b) {
    FileEntry* fa = (FileEntry*)a;
    FileEntry* fb = (FileEntry*)b;
    if (fa->is_dir && !fb->is_dir) return -1;
    if (!fa->is_dir && fb->is_dir) return 1;
    return strcasecmp(fa->name, fb->name);
}

void populate_file_list() {
    file_count = 0;
    DIR* dir = opendir(current_path);
    if (!dir) {
        strcpy(current_path, "sdmc:/");
        dir = opendir(current_path);
        if (!dir) return;
    }

    if (strcmp(current_path, "sdmc:/") != 0) {
        strcpy(file_list[file_count].name, "..");
        file_list[file_count].is_dir = true;
        file_count++;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && file_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        bool is_dir = (entry->d_type == DT_DIR);
        const char* ext = strrchr(entry->d_name, '.');
        if (is_dir || (ext && strcasecmp(ext, ".bmp") == 0)) {
            strncpy(file_list[file_count].name, entry->d_name, MAX_FILENAME_LEN - 1);
            file_list[file_count].name[MAX_FILENAME_LEN - 1] = '\0';
            file_list[file_count].is_dir = is_dir;
            file_count++;
        }
    }
    closedir(dir);

    if (file_count > 0) {
        qsort(file_list, file_count, sizeof(FileEntry), compare_files);
    }
    cursor = 0;
    scroll_offset = 0;
}

void draw_ui() {
    consoleClear();

    consoleSelect(&topScreen);
    printf("\x1b[2;11H-- BMP to JPG Converter --");
    printf("\x1b[4;3HSelect a .bmp file to convert it to .jpg");
    printf("\x1b[5;8Hin the same folder.");
    printf("\x1b[28;1H(A) Select | (B) Back/Up | (START) Exit");

    consoleSelect(&bottomScreen);
    printf("\x1b[1;1HCurrent: %.32s", current_path);

    int max_visible_items = BOTTOM_SCREEN_ROWS - 3;
    for (int i = 0; i < max_visible_items; i++) {
        int index = scroll_offset + i;
        if (index >= file_count) break;
        printf("\x1b[%d;1H", 3 + i);

        if (index == cursor) printf("\x1b[32m> ");
        else printf("  ");

        if (file_list[index].is_dir) printf("\x1b[34m%s\x1b[0m", file_list[index].name);
        else printf("\x1b[37m%s\x1b[0m", file_list[index].name);
    }
}

void move_cursor(int direction) {
    if (file_count == 0) return;

    cursor += direction;

    if (cursor >= file_count) cursor = 0;
    if (cursor < 0) cursor = file_count - 1;

    int max_visible_items = BOTTOM_SCREEN_ROWS - 3;
    if (cursor < scroll_offset) {
        scroll_offset = cursor;
    }
    if (cursor >= scroll_offset + max_visible_items) {
        scroll_offset = cursor - max_visible_items + 1;
    }
}

bool handle_action_keys(u32 kDown) {
    if (kDown & KEY_A) {
        if (file_count > 0 && cursor >= 0 && cursor < file_count) {
            FileEntry selected = file_list[cursor];
            if (selected.is_dir) {
                if (strcmp(selected.name, "..") == 0) {
                    char* last_slash = strrchr(current_path, '/');
                    if (last_slash > current_path && last_slash != strstr(current_path, ":/")) {
                         *(last_slash) = '\0';
                         last_slash = strrchr(current_path, '/');
                         *(last_slash + 1) = '\0';
                    }
                } else {
                    strcat(current_path, selected.name);
                    strcat(current_path, "/");
                }
                return true;
            } else {
                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s%s", current_path, selected.name);
                convert_image(full_path);
            }
        }
    }

    if (kDown & KEY_B) {
        if (strcmp(current_path, "sdmc:/") != 0) {
             char* last_slash = strrchr(current_path, '/');
             if (last_slash > current_path && last_slash != strstr(current_path, ":/")) {
                  *(last_slash) = '\0';
                  last_slash = strrchr(current_path, '/');
                  *(last_slash + 1) = '\0';
             }
             return true;
        }
    }

    return false;
}

bool convert_image(const char* input_path) {
    show_message("Processing... Please wait.", false);
    int width, height, bpp;
    unsigned char* bmp_data = stbi_load(input_path, &width, &height, &bpp, 3);
    if (!bmp_data) {
        show_message("Error: Failed to load BMP!", true);
        return false;
    }
    
    char output_path[1024];
    strcpy(output_path, input_path);
    char* ext = strrchr(output_path, '.');
    if (ext) strcpy(ext, ".jpg");
    else strcat(output_path, ".jpg");

    int success = stbi_write_jpg(output_path, width, height, 3, bmp_data, 85);
    stbi_image_free(bmp_data);

    if (!success) {
        show_message("Error: Failed to write JPG.", true);
        return false;
    }
    show_message("Conversion successful!", true);
    return true;
}

void show_message(const char* message, bool wait_for_key) {
    consoleClear();
    consoleSelect(&topScreen);
    int msg_len = strlen(message);
    int col = (TOP_SCREEN_COLS - msg_len) / 2;
    printf("\x1b[14;%dH%s", col > 0 ? col : 1, message);

    if (wait_for_key) printf("\x1b[16;16HPress (A) to continue.");
    
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();

    if (wait_for_key) {
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_A) break;
            gspWaitForVBlank();
        }
    } else {
        svcSleepThread(1000000000);
    }
}

int main(int argc, char* argv[]) {
    gfxInitDefault();
    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

    populate_file_list();

    bool needs_redraw = true;

    u64 repeat_timer = 0;
    u32 repeating_key = 0;

    while (aptMainLoop()) {
        hidScanInput();

        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        u32 kUp = hidKeysUp();

        if (kDown & KEY_START) break;

        if (kDown & (KEY_UP | KEY_DOWN)) {
            if (kDown & KEY_UP) move_cursor(-1);
            if (kDown & KEY_DOWN) move_cursor(1);
            
            repeating_key = kDown & (KEY_UP | KEY_DOWN);
            repeat_timer = osGetTime() + REPEAT_START_DELAY;
            needs_redraw = true;
        }
        
        if (repeating_key && (kHeld & repeating_key)) {
            if (osGetTime() > repeat_timer) {
                if (kHeld & KEY_UP) move_cursor(-1);
                if (kHeld & KEY_DOWN) move_cursor(1);
                
                repeat_timer = osGetTime() + REPEAT_CONTINUE_DELAY;
                needs_redraw = true;
            }
        }

        if (kUp & repeating_key) {
            repeating_key = 0;
            repeat_timer = 0;
        }

        if (handle_action_keys(kDown)) {
            populate_file_list();
            needs_redraw = true;
        } else if (kDown & (KEY_A | KEY_B)) {
            needs_redraw = true;
        }

        if (needs_redraw) {
            draw_ui();
            needs_redraw = false;
        }
        
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}