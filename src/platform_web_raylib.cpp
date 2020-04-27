
#include "raylib.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif


#include <cstdlib>
#include "math.h"
#include <limits>
#include <type_traits>
#include "pacman_platform.h"
namespace Game {
#include <type_traits>
#include "pacman.cpp"
};


#define TILE_SIZE  32
#define WIDTH (21 * TILE_SIZE)
#define HEIGHT (27 * TILE_SIZE)

#define SCREEN_WIDTH   (WIDTH)  // 800
#define SCREEN_HEIGHT  (HEIGHT) // 600


PLATFORM_FREE_FILE_MEMORY_SIG(platform_free_file_memory) {
    if (!memory)  return;
    free(memory);
}

PLATFORM_READ_ENTIRE_FILE_SIG(platform_read_entire_file) {
    Read_File_Result result = {};

	unsigned int bytes_read;
	u8 *data = LoadFileData(filename, &bytes_read);
	
	if (bytes_read <= 0) {
		if (data) {
			free(data);
		}
		return {};
	}
	
	result.memory_size = bytes_read;
	result.memory = data;
    
    return result;
}


internal void
platform_process_key_press(Game_Button_State *new_state, b32 is_down) {
    // assert(new_state->ended_down != is_down);
    
    if (new_state->ended_down == is_down)  return;
    new_state->ended_down = is_down;
    ++new_state->half_transition_count;
}

internal void
platform_process_pending_events(Game_Input *new_input) {
	Game_Controller_Input *new_keyboard_controller = &new_input->controllers[0];
	platform_process_key_press(&new_keyboard_controller->move_up,        IsKeyDown(KEY_W));
	platform_process_key_press(&new_keyboard_controller->move_left,      IsKeyDown(KEY_A));
	platform_process_key_press(&new_keyboard_controller->move_down,      IsKeyDown(KEY_S));
	platform_process_key_press(&new_keyboard_controller->move_right,     IsKeyDown(KEY_D));
	platform_process_key_press(&new_keyboard_controller->left_shoulder,  IsKeyDown(KEY_Q));
	platform_process_key_press(&new_keyboard_controller->right_shoulder, IsKeyDown(KEY_E));
	platform_process_key_press(&new_keyboard_controller->move_up,        IsKeyDown(KEY_UP));
	platform_process_key_press(&new_keyboard_controller->move_left,      IsKeyDown(KEY_LEFT));
	platform_process_key_press(&new_keyboard_controller->move_down,      IsKeyDown(KEY_DOWN));
	platform_process_key_press(&new_keyboard_controller->move_right,     IsKeyDown(KEY_RIGHT));
	platform_process_key_press(&new_keyboard_controller->action_down,    IsKeyDown(KEY_K));
	platform_process_key_press(&new_keyboard_controller->action_right,   IsKeyDown(KEY_J));
	platform_process_key_press(&new_keyboard_controller->start,          IsKeyDown(KEY_ENTER));
	platform_process_key_press(&new_keyboard_controller->back,           IsKeyDown(KEY_BACKSPACE));

}


global Game_Input input[2] = {};
global Game_Input *new_input = &input[0];
global Game_Input *old_input = &input[1];

global Game_Memory game_memory;
global Texture2D target_texture;
global Image target_image;
	
void platform_update_and_render_frame() {
	new_input->dt = GetFrameTime();
	int window_width = GetScreenWidth();
	int window_height = GetScreenHeight();
	Game::Rectangle_s32 draw_region = Game::aspect_ratio_fit(WIDTH, HEIGHT, window_width, window_height);
	
    //
    // @note handle input
    //
    Game_Controller_Input *old_keyboard_controller = &old_input->controllers[0];
    Game_Controller_Input *new_keyboard_controller = &new_input->controllers[0];
    *new_keyboard_controller = {};
    for (int button_index = 0; button_index < array_count(new_keyboard_controller->buttons); ++button_index) {
        new_keyboard_controller->buttons[button_index].ended_down = old_keyboard_controller->buttons[button_index].ended_down;
    }
        
    platform_process_pending_events(new_input);
#if !defined(PLATFORM_WEB)
    if (WindowShouldClose())  return;
#endif
	
	// @note handle mouse
	int int_mouse_x = GetMouseX();
    int int_mouse_y = GetMouseY();
	f32 mouse_x = (f32)int_mouse_x;
    f32 mouse_y = (f32)int_mouse_y;
	new_input->mouse_x = Game::clamp_01_map_to_range((f32)draw_region.min_x, mouse_x, (f32)draw_region.max_x) * WIDTH;
    new_input->mouse_y = Game::clamp_01_map_to_range((f32)draw_region.min_y, mouse_y, (f32)draw_region.max_y) * HEIGHT;
	new_input->mouse_y = new_input->mouse_y;
    new_input->mouse_z = 0; // @todo Support mousewheel?
        
    platform_process_key_press(&new_input->mouse_buttons[0], IsMouseButtonDown(MOUSE_LEFT_BUTTON));
    platform_process_key_press(&new_input->mouse_buttons[1], IsMouseButtonDown(MOUSE_MIDDLE_BUTTON));
    platform_process_key_press(&new_input->mouse_buttons[2], IsMouseButtonDown(MOUSE_RIGHT_BUTTON));
    // @todo platform_process_key_press(&new_input->mouse_buttons[3], IsMouseButtonDown(BUTTON_X1));
    // @todo platform_process_key_press(&new_input->mouse_buttons[4], IsMouseButtonDown(BUTTON_X2));
	
	//
	// @note: Update and Render
	//
    BeginDrawing();
    {
        ClearBackground(BLACK);
		//BeginTextureMode(target_texture);
		//ClearBackground(BLACK);
		
		
		//UnloadRenderTexture(target_texture);
		//Image target_image = GetTextureData(target_texture.texture);
		assert(target_image.format == UNCOMPRESSED_R8G8B8A8);
		
		Game_Offscreen_Buffer buffer = {};
        buffer.memory = target_image.data;
		assert(buffer.memory);
        buffer.width  = target_image.width;
        buffer.height = target_image.height;
        buffer.bytes_per_pixel = 4;
        buffer.pitch  = buffer.width * buffer.bytes_per_pixel;
        Game::game_update_and_render(&game_memory, new_input, &buffer);
        if (new_input->request_quit)  {
            // @todo global_running = false;
        }
		{
			// @note Swizzle the colors around because of wrong pixel format.
			uint32_t *pixel = (uint32_t *)buffer.memory;
			size_t pixel_count = buffer.width * buffer.height;
			for (size_t i = 0; i < pixel_count; ++i) {
				uint8_t a = (*pixel >> 24) & 0xFF;
				uint8_t r = (*pixel >> 16) & 0xFF;
				uint8_t g = (*pixel >>  8) & 0xFF;
				uint8_t b = (*pixel >>  0) & 0xFF;
				*pixel = ((uint32_t)(r << 24) |
						  (uint32_t)(b << 16) |
						  (uint32_t)(g <<  8) |
						  (uint32_t)(a <<  0));
				// src = 0x AA RR GG BB
				// WTF??? dest = 0x RR BB GG AA
				++pixel;
			}
		}
		//UpdateTexture(target_texture.texture, target_image.data);
		
		//Texture2D texture = LoadTextureFromImage(target_image);
		UpdateTexture(target_texture, target_image.data);
		DrawTexture(target_texture, 0, 0, WHITE);
		
		
		//DrawText("Hello Sailor!", 10, 50, 50, WHITE);
		DrawFPS(SCREEN_WIDTH - 100, 0);
		
		
		//EndTextureMode();
		/*
#define min(a, b) ((a)<(b)? (a) : (b))
		float scale = min((float)GetScreenWidth()/(float)SCREEN_WIDTH, (float)GetScreenHeight()/(float)SCREEN_HEIGHT);
		Rectangle rect_src = { 
			0.0f, 
			0.0f, 
			(float)target_texture.texture.width, 
			(float)-target_texture.texture.height 
		};
		Rectangle rect_dest = { 
			(float)((float)GetScreenWidth() - ((float)SCREEN_WIDTH*scale))*0.5f, 
			(float)((float)GetScreenHeight() - ((float)SCREEN_HEIGHT*scale))*0.5f,
            (float)SCREEN_WIDTH * scale,
			(float)SCREEN_HEIGHT * scale 
		};
		*/
		//DrawTexturePro(target_texture.texture, rect_src, rect_dest, Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
	}
    EndDrawing();
	
	
	Game_Input *temp_input = new_input;
    new_input = old_input;
    old_input = temp_input;
}

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "raylib_playground");
	SetWindowSize(SCREEN_WIDTH, SCREEN_HEIGHT);
	
	game_memory = {};
    game_memory.platform_free_file_memory = platform_free_file_memory;
    game_memory.platform_read_entire_file = platform_read_entire_file;
	
	game_memory.permanent_storage_size = megabytes_to_bytes(32);
	game_memory.permanent_storage = malloc(game_memory.permanent_storage_size);
	assert(game_memory.permanent_storage);
	
	
	//target_texture = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    //SetTextureFilter(target_texture.texture, FILTER_BILINEAR);  // Texture scale filter to use
	target_image = GenImageColor(SCREEN_WIDTH, SCREEN_HEIGHT, BLANK);
	ImageFormat(&target_image, UNCOMPRESSED_R8G8B8A8);
	target_texture = LoadTextureFromImage(target_image);
	
	
#if defined(PLATFORM_WEB)
	emscripten_set_main_loop(platform_update_and_render_frame, 0, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
		platform_update_and_render_frame();
    }
#endif
	
	//UnloadRenderTexture(target_texture);
    CloseWindow(); // Close window and OpenGL context
    
    return 0;
}
