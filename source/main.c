#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <3ds.h>
#include <citro2d.h>


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MAX_FILES 256
#define MAX_FILENAME_LEN 256
#define TOP_SCREEN_WIDTH 400
#define BOTTOM_SCREEN_WIDTH 320

typedef struct {
    char name[MAX_FILENAME_LEN];
    bool is_dir;
} FileEntry;

static FileEntry file_list[MAX_FILES];
static int file_count = 0;
static int cursor = 0;
static int scroll_offset = 0;
static char current_path[1024] = "sdmc:/";
static C3D_RenderTarget* top;
static C3D_RenderTarget* bottom;
static C2D_TextBuf g_text_buf;
static C2D_Text g_file_text[20];

void populate_file_list();
void draw_ui();
bool handle_input();
bool convert_image(const char* input_path);
void show_message(const char* message, bool wait_for_key);

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
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

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
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(top, C2D_Color32(0x1E, 0x1E, 0x1E, 0xFF));
    C2D_SceneBegin(top);

    C2D_TextBufClear(g_text_buf);
    C2D_TextParse(&g_file_text[0], g_text_buf, "BMP to JPG Converter (Old 3DS Optimized)");
    C2D_TextOptimize(&g_file_text[0]);
    C2D_DrawText(&g_file_text[0], C2D_AlignCenter, 10.0f, 0.5f, 0.5f, 0.5f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));

    C2D_TextBufClear(g_text_buf);
    C2D_TextParse(&g_file_text[0], g_text_buf, "D-Pad: Navigate | (A) Select | (B) Back | START: Exit");
    C2D_TextOptimize(&g_file_text[0]);
    C2D_DrawText(&g_file_text[0], C2D_AlignLeft | C2D_AtBaseline, 8.0f, 220.0f, 0.5f, 0.5f, 0.5f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));

    C2D_TextBufClear(g_text_buf);
    C2D_TextParse(&g_file_text[0], g_text_buf, "Select a .bmp file to convert it to .jpg in the same folder.");
    C2D_TextOptimize(&g_file_text[0]);
    C2D_DrawText(&g_file_text[0], C2D_AlignLeft | C2D_AtBaseline, 8.0f, 40.0f, 0.5f, 0.5f, 0.5f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));

    C2D_TargetClear(bottom, C2D_Color32(0x33, 0x33, 0x33, 0xFF));
    C2D_SceneBegin(bottom);

    C2D_TextBufClear(g_text_buf);
    char display_path[60];
    snprintf(display_path, 60, "Current: %s", current_path);
    C2D_TextParse(&g_file_text[0], g_text_buf, display_path);
    C2D_TextOptimize(&g_file_text[0]);
    C2D_DrawText(&g_file_text[0], C2D_AlignLeft, 5.0f, 5.0f, 0.5f, 0.5f, 0.5f, C2D_Color32(0xFF, 0xCC, 0x00, 0xFF));

    int max_visible_items = 12;
    for (int i = 0; i < max_visible_items; i++) {
        int index = scroll_offset + i;
        if (index >= file_count) break;

        u32 color = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
        if (file_list[index].is_dir) {
            color = C2D_Color32(0x7F, 0xC4, 0xEB, 0xFF);
        }
        if (index == cursor) {
            color = C2D_Color32(0x00, 0xFF, 0x00, 0xFF);
        }

        C2D_TextBufClear(g_text_buf);
        C2D_TextParse(&g_file_text[i], g_text_buf, file_list[index].name);
        C2D_TextOptimize(&g_file_text[i]);
        C2D_DrawText(&g_file_text[i], C2D_AlignLeft, 25.0f + (index == cursor ? 10.0f : 0.0f), 30.0f + (i * 15.0f), 0.5f, 0.5f, 0.5f, color);
    }
     if (cursor >= 0 && cursor < file_count) {
         C2D_TextBufClear(g_text_buf);
         C2D_TextParse(&g_file_text[19], g_text_buf, ">");
         C2D_TextOptimize(&g_file_text[19]);
         C2D_DrawText(&g_file_text[19], C2D_AlignLeft, 15.0f, 30.0f + ((cursor - scroll_offset) * 15.0f), 0.5f, 0.5f, 0.5f, C2D_Color32(0x00, 0xFF, 0x00, 0xFF));
     }
    C3D_FrameEnd(0);
}

bool handle_input() {
    hidScanInput();
    u32 kDown = hidKeysDown();

    if (kDown & KEY_START) {
        return true;
    }

    if (kDown & KEY_DOWN) {
        cursor++;
        if (cursor >= file_count) {
            cursor = 0;
        }
    }
    if (kDown & KEY_UP) {
        cursor--;
        if (cursor < 0) {
            cursor = file_count > 0 ? file_count - 1 : 0;
        }
    }

    int max_visible_items = 12;
    if (cursor < scroll_offset) {
        scroll_offset = cursor;
    }
    if (cursor >= scroll_offset + max_visible_items) {
        scroll_offset = cursor - max_visible_items + 1;
    }

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
                populate_file_list();
            } else {
                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s%s", current_path, selected.name);
                if (convert_image(full_path)) {
                    return true;
                }
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
             populate_file_list();
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
    if (ext) {
        strcpy(ext, ".jpg");
    } else {
        strcat(output_path, ".jpg");
    }

    int success = stbi_write_jpg(output_path, width, height, 3, bmp_data, 85);

    stbi_image_free(bmp_data);

    if (!success) {
        show_message("Error: Failed to write JPG.", true);
        return false;
    }

    return true;
}

void show_message(const char* message, bool wait_for_key) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(top, C2D_Color32(0x1E, 0x1E, 0x1E, 0xFF));
    C2D_SceneBegin(top);

    C2D_TextBufClear(g_text_buf);
    C2D_TextParse(&g_file_text[0], g_text_buf, message);
    C2D_TextOptimize(&g_file_text[0]);
    C2D_DrawText(&g_file_text[0], C2D_AlignCenter, 120.0f, 0.5f, 0.5f, 0.5f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));

    if (wait_for_key) {
        C2D_TextBufClear(g_text_buf);
        C2D_TextParse(&g_file_text[1], g_text_buf, "Press (A) to continue.");
        C2D_TextOptimize(&g_file_text[1]);
        C2D_DrawText(&g_file_text[1], C2D_AlignCenter, 160.0f, 0.5f, 0.5f, 0.5f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));
    }

    C3D_FrameEnd(0);
    
    if (wait_for_key) {
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_A) break;
            gspWaitForVBlank();
        }
    } else {
        gspWaitForVBlank();
    }
}

int main(int argc, char* argv[]) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_text_buf = C2D_TextBufNew(4096);
    populate_file_list();

    while (aptMainLoop()) {
        
        if (handle_input()) {
            break; 
        }

        draw_ui();
        
    }

    C2D_TextBufDelete(g_text_buf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();

    return 0;
}