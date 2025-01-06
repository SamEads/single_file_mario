#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <raylib.h>
#include <rlgl.h>
#include <stb_rect_pack.h>

#pragma region Defines

#define DEV
#define LOG_PRINT true

// resource related defines
#ifdef EDIT_MODE
#define WINDOW_CAPTION 			"Solar Sorcery"
#else
#define WINDOW_CAPTION 			"Super Mario World"
#endif
#define SPRITES_PATH 			"assets/sprites"
#define SOUNDS_PATH 			"assets/sounds"
#define BACKGROUNDS_PATH 		"assets/backgrounds"
#define MAX_PATH_LEN 256

#define GAME_WIDTH 			256
#define GAME_HEIGHT 		224

// game related defines
#define MAX_CONTROLLERS 4
#define ENTITY_DEFAULT_ALLOCATION_SIZE 64

// player speeds
#define PLAYER_WALK_SPEED 		1.25f
#define PLAYER_RUN_SPEED 		2.25f
#define PLAYER_JUMP				5.0f
#define PLAYER_DECEL			(1.0f / 16.0f)
#define PLAYER_DECEL_AIR 		(1.0f / 80.0f)
#define PLAYER_ACCEL			0.09375f
#define PLAYER_TURN 			0.15625f
#define PLAYER_TURN_AIR			0.15625f
#define PLAYER_GRAVITY			0.375f
#define PLAYER_GRAVITY_HOLD		0.1875f

// background colors
#define ORANGE_SKY	(Color) { 255, 231, 181, 255 }
#define BLUE_SKY	(Color) { 0, 99, 189, 255 }
#define BLACK_SKY	(Color) { 0, 0, 0, 255 }

#define EDITOR_GRADIENT_TOP 	((Color) { .b = 96, .a = 255 })
#define EDITOR_GRADIENT_BOTTOM 	((Color) { .b = 255, .a = 255 })

// texture atlas defines
#define TEXTURE_ATLAS_WIDTH 	512
#define TEXTURE_ATLAS_HEIGHT 	512
#define MAX_TEXTURE_NODES 		1024

#define TILE_ATLAS_WIDTH 		1024
#define TILE_ATLAS_HEIGHT 		1024
#define DEFAULT_TILE_SIZE 		16
#define MAX_TILE_NODES 			8192

// array list defines
#define ARRAYLIST_NULL -1
#define ARRAYLIST_SCALE_FACTOR 2

#ifdef DEV
#define printd(...) if (LOG_PRINT) printf(__VA_ARGS__);
#else
#define printd(...)
#endif

#define recast(target_type, ptr) \
	(target_type*)((char*)(ptr) - offsetof(target_type, base))

#pragma endregion

#pragma region Forward Declares

struct controller_state;
typedef struct controller_state controller_state_t;

struct game;
typedef struct game game_t;

struct render_context;
typedef struct render_context render_context_t;

struct entity;
typedef struct entity entity_t;

struct physics_body;
typedef struct physics_body physics_body_t;

struct sprite_frame;
typedef struct sprite_frame sprite_frame_t;

#ifdef EDIT_MODE
struct editor;
typedef struct editor editor_t;
#endif

struct level;
typedef struct level level_t;
void level_update(level_t*, game_t*);
void level_draw(level_t*, render_context_t*);

struct player;
typedef struct player player_t;
void player_init(player_t*);
void player_update(player_t*, level_t*, controller_state_t*);
void player_draw(player_t*, level_t*, render_context_t*);

#pragma endregion

#pragma region Mathstuffs

#undef min
#undef max
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(v, a, b) (MAX(MIN(v, b), a))
#define RAND_INT(min, max) (min + rand() / (RAND_MAX / (max - min + 1) + 1))

double distance(double x1, double y1, double x2, double y2) {
    double square_difference_x = (x2 - x1) * (x2 - x1);
    double square_difference_y = (y2 - y1) * (y2 - y1);
    double sum = square_difference_x + square_difference_y;
    double value = sqrt(sum);
    return value;
}

#pragma endregion

#pragma region IO

/**
 * Indexes a path from a consistent format
 * @param src_loc	Original source, i.e. "player.idle"
 * @param dest_loc	Destination source, i.e. "player/idle"
 */
void path_index(const char* src_loc, char* dest_loc) {
	int fname_len = strlen(src_loc);
	memcpy(dest_loc, src_loc, (size_t)(fname_len + 1));
	// iterate thru each char and swap chars
	for (int i = 0; i < fname_len; ++i) {
		if (dest_loc[i] == '.') {
			dest_loc[i] = '/';
		}
	}
	// re-add delimiter
	dest_loc[fname_len] = '\0';
}

#pragma endregion

#pragma region Rectangle bounds

bool point_in_rectangle(Vector2 p, Rectangle r) {
	return ((p.x <= (r.x + r.width) && p.x >= r.x) && (p.y <= (r.y + r.height) && p.y >= r.y));
}

bool rectangle_collision(Rectangle r1, Rectangle r2) {
	return ((r1.x < (r2.x + r2.width) && (r1.x + r1.width) > r2.x) &&
			(r1.y < (r2.y + r2.height) && (r1.y + r1.height) > r2.y));
}

bool rectangle_collision_list(Rectangle base_rect, Rectangle* recs, int num_rects) {
	for (int i = 0; i < num_rects; ++i) {
		if (rectangle_collision(base_rect, recs[i])) {
			return true;
		}
	}
	return false;
}

#pragma endregion

#pragma region GUI
#ifdef DEV

#define WINDOW_CLOSEBUTTON_SIZE 	18
#define WINDOW_MINIMUM_WIDTH 		100
#define WINDOW_MINIMUM_HEIGHT 		100
#define WINDOW_STATUSBAR_HEIGHT		24

#ifndef RAYGUI_IMPLEMENTATION
#define RAYGUI_IMPLEMENTATION
#include <raygui/raygui.h>
#include <raygui/styles/dark/style_dark.h>

typedef struct window {
	bool focused;
} window_t;

typedef struct floating_window {
	window_t base;
	char title[100];
	Vector2 position;
	Vector2 window_size;
	bool minimized;
	bool moving;
	bool resizing;
	bool resizable;
	bool force_scroll_y;
	void (*draw_content)(struct floating_window*, Vector2, Vector2, float);
	Vector2 scroll;
} floating_window_t;

/**
 * Places window back within screen bounds once the window is unreachable.
 * @param window Window to fit to screen
 */
void window_fit_to_screen(floating_window_t* window) {
	if (window->position.x + window->window_size.x < 32) {
		window->position.x = 0;
	}
	else if (window->position.x > GetScreenWidth() - 32) {
		window->position.x = GetScreenWidth() - window->window_size.x;
	}
	if (window->position.y < 0) {
		window->position.y = 0;
	}
	else if (window->position.y > GetScreenHeight() - 24) {
		window->position.y = GetScreenHeight() - 24;
	}
}

// https://github.com/raysan5/raygui/blob/master/examples/floating_window/floating_window.c
void window_run(floating_window_t* window, float delta) {
	bool prev_window_focus = window->base.focused;
	window->base.focused = false;
	if (window->minimized) {
		return;
	}

	Vector2 mouse_position = GetMousePosition();
	Rectangle resize_area = { window->position.x + window->window_size.x - 20, window->position.y + window->window_size.y - 20 + 24, 25, 25 };
	
	bool in_resize_range = point_in_rectangle(mouse_position, resize_area);

	if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
		if (!window->moving && !window->resizing) {
			Rectangle title_collision_rect = {
				window->position.x, window->position.y,
				window->window_size.x, RAYGUI_WINDOWBOX_STATUSBAR_HEIGHT
			};

			if (point_in_rectangle(mouse_position, title_collision_rect)) {
				window->moving = true;
			}
			else if (window->resizable && !window->minimized && in_resize_range) {
				window->resizing = true;
			}
		}
	}

	// move window around if the cursor is dragging it
	if (window->moving) {
		Vector2 mouse_delta = GetMouseDelta();

		window->position.x += mouse_delta.x;
		window->position.y += mouse_delta.y;

		// release window
		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
			window->moving = false;
			window_fit_to_screen(window);
		}
	}
	else if (window->resizing) {
		if (mouse_position.x > window->position.x)
			window->window_size.x = mouse_position.x - window->position.x;
		if (mouse_position.y > window->position.y - WINDOW_STATUSBAR_HEIGHT)
			window->window_size.y = mouse_position.y - window->position.y - WINDOW_STATUSBAR_HEIGHT;

		// clamp window size so not to be too small
		if (window->window_size.x < WINDOW_MINIMUM_WIDTH) window->window_size.x = WINDOW_MINIMUM_WIDTH;
		else if (window->window_size.x > GetScreenWidth()) window->window_size.x = GetScreenWidth();
		if (window->window_size.y < WINDOW_MINIMUM_HEIGHT + WINDOW_STATUSBAR_HEIGHT) window->window_size.y = WINDOW_MINIMUM_HEIGHT + WINDOW_STATUSBAR_HEIGHT;
		else if (window->window_size.y > GetScreenHeight()) window->window_size.y = GetScreenHeight();

		if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
			window->resizing = false;
		}
	}
	else {
		window_fit_to_screen(window);
	}

	Rectangle statusbar_area = 	{ (int)window->position.x, (int)window->position.y, (int)window->window_size.x, WINDOW_STATUSBAR_HEIGHT };
	Rectangle content_area = 	{ (int)window->position.x, (int)window->position.y + WINDOW_STATUSBAR_HEIGHT, (int)window->window_size.x, (int)window->window_size.y };

	if (window->moving || window->resizing) {
		window->base.focused = false;
	}
	else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && point_in_rectangle(mouse_position, content_area)) {
		window->base.focused = true;
	}

	// top status bar
	GuiStatusBar(statusbar_area, window->title);

	// window content area
	GuiPanel(content_area, NULL);

	// window has a custom content drawing operation
	if (window->draw_content != NULL) {
		int off_x = (int) window->position.x + 1;
		int off_y = (int) window->position.y + WINDOW_STATUSBAR_HEIGHT;

		float mp_x = mouse_position.x - off_x;
		float mp_y = mouse_position.y - off_y;

		// window content clip
		BeginScissorMode(off_x, off_y, window->window_size.x - 2, window->window_size.y - 1 );

		rlPushMatrix();
		rlTranslatef(off_x, off_y, 0.0f);
		window->draw_content(window, (Vector2) { window->window_size.x - 2, window->window_size.y - 1 }, (Vector2) { mp_x, mp_y }, delta);
		rlPopMatrix();

		EndScissorMode();
	}

	// Set cursor types
	if (window->resizing || in_resize_range) {
		// resize cursor
		SetMouseCursor(MOUSE_CURSOR_RESIZE_NWSE);
	} else {
		// default cursor
		SetMouseCursor(MOUSE_CURSOR_DEFAULT);
	}
}

typedef struct tilemap_window {
	floating_window_t base;
	Texture2D tilemap;
	int timer;
	Vector2 selected_tile;
} tilemap_window_t;

void draw_window_selection(Rectangle bounds, int timer, int freq, Color line_color_a, Color line_color_b) {
    int _timer = timer;
    Color color_top, color_left, color_right, color_bottom;

    for (int i = 0; i < bounds.width; i++) {
        color_top = ((((_timer / (freq * 2)) - i) & ((freq * 2) - 1)) < freq) ? line_color_a : line_color_b;
        color_bottom = ((((_timer / (freq * 2)) + i) & ((freq * 2) - 1)) < freq) ? line_color_a : line_color_b;
        DrawRectangle(bounds.x + i, bounds.y, 1, 1, color_top);								// top
        DrawRectangle(bounds.x + i, bounds.y + (bounds.height - 1), 1, 1, color_bottom);	// bottom
    }

    for (int j = 0; j < bounds.height; j++) {
        color_left = ((((_timer / (freq * 2)) + j) & ((freq * 2) - 1)) < freq) ? line_color_a : line_color_b;
        color_right = ((((_timer / (freq * 2)) - j) & ((freq * 2) - 1)) < freq) ? line_color_a : line_color_b;
        DrawRectangle(bounds.x, bounds.y + j, 1, 1, color_left);						// left
        DrawRectangle(bounds.x + (bounds.width - 1), bounds.y + j, 1, 1, color_right);	// right
    }
}

/**
 * Runs once-per-frame when the tilemap window is open
 * @param window 			Window to draw
 * @param size 				Window size
 * @param mouse_position 	Local mouse position
 */
void tilemap_window_draw_content(floating_window_t* window, Vector2 size, Vector2 mouse_position, float delta) {
	// cast to tilemap window
	tilemap_window_t* tilemap_window = recast(tilemap_window_t, window);

	// background gradient fill
	DrawRectangleGradientV(0, 0, size.x, size.y, EDITOR_GRADIENT_TOP, EDITOR_GRADIENT_BOTTOM);
	
	// tilemap texture
	DrawTexture(tilemap_window->tilemap, 0, 0, WHITE);
	
	if (window->base.focused && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		tilemap_window->selected_tile = (Vector2) { (int)(mouse_position.x / (float)DEFAULT_TILE_SIZE), (int)(mouse_position.y / (float)DEFAULT_TILE_SIZE) };
	}

	// hovered tile position
	int grid_x = (int)(mouse_position.x / (float)DEFAULT_TILE_SIZE);
	int grid_y = (int)(mouse_position.y / (float)DEFAULT_TILE_SIZE);
	
	if (point_in_rectangle(mouse_position, (Rectangle) { 0, 0, size.x, size.y })) {
		// mouse cursor doesnt match tile- draw red selection
		if (grid_x != tilemap_window->selected_tile.x || grid_y != tilemap_window->selected_tile.y) {
			DrawRectangle(grid_x * DEFAULT_TILE_SIZE, grid_y * DEFAULT_TILE_SIZE, 16, 16, (Color) {255, 90, 90, 128});
		}
		// mouse cursor matches highlighted tile- draw overlay
		else {
			// draw inverted rectangle inside of selected tile bounds
			if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
				BeginBlendMode(BLEND_SUBTRACT_COLORS);
				DrawRectangle(tilemap_window->selected_tile.x * (float)DEFAULT_TILE_SIZE, tilemap_window->selected_tile.y * (float)DEFAULT_TILE_SIZE, DEFAULT_TILE_SIZE, DEFAULT_TILE_SIZE, (Color) { 255, 255, 255, 0 });
				EndBlendMode();
			}

		}
	}
	draw_window_selection((Rectangle) { tilemap_window->selected_tile.x * (float)DEFAULT_TILE_SIZE, tilemap_window->selected_tile.y * (float)DEFAULT_TILE_SIZE, DEFAULT_TILE_SIZE, DEFAULT_TILE_SIZE }, delta * 2.0f, 4, WHITE, BLUE);

	// soft shadow overlay at the bottom of the window
	DrawRectangleGradientV(0, size.y - 64, size.x, 64, (Color) { 0 }, (Color) { .a = 64 });

	++tilemap_window->timer;
}

#ifdef EDIT_MODE

struct editor {
	float zoom;
	Texture2D background_texture;
	tilemap_window_t tiles_window;
	floating_window_t entities_window;
	floating_window_t* active_windows[16]; // hardcoded maximum of 16 open windows- maybe move to heap alloc if this gets crazy but this is fine for now
	int active_window_count;
	bool dragging;
	float selection_box_timer;
	Vector2 drag_begin;
	Vector2 drag_end;
	char footer_info[512];
	window_t* focused_window;
};

void editor_init(editor_t* editor) {
	*editor = (editor_t) {
		.zoom = 1.0f,	// default zoom of 100%
		.tiles_window = {
			// floating window base
			.base = (floating_window_t) {
				.base = { 0 },
				.title = "16x16 Tile Selector",
				.draw_content = tilemap_window_draw_content,
				.resizable = true,
				.position = (Vector2) { 0 },
				.window_size = (Vector2) { 256, 256 },
			},
			// tilemap to render
			.tilemap = LoadTexture("assets/tiles/glade.png")
		},
		.background_texture = LoadTexture("assets/backgrounds/overworld.png"),
		.active_window_count = 0
	};
	SetTextureWrap(editor->background_texture, TEXTURE_WRAP_REPEAT);
}

void editor_toolbar(editor_t* editor, const int toolbar_width, const int toolbar_height, const char** labels, const int label_count) {
	// toolbar rectangle
	GuiDrawRectangle((Rectangle) { .width = toolbar_width, .height = toolbar_height }, GuiGetStyle(STATUSBAR, BORDER_WIDTH), GetColor(GuiGetStyle(STATUSBAR, BORDER)), GetColor(GuiGetStyle(STATUSBAR, BASE)));

	// draw each label
	const int label_offset = 12;
	for (int i = 0, current_x_offset = 8; i < label_count; ++i) {
		const int text_width = GetTextWidth(labels[i]);
		GuiLabel((Rectangle) { current_x_offset, 0, text_width, toolbar_height }, labels[i]);
		if (i < label_count - 1) {
			// increment label spacing, if necessary
			current_x_offset += text_width + label_offset;
		}
	}
}

void editor_set_focused_window(editor_t* editor, window_t* window) {
	editor->focused_window = window;
}

void editor_viewport(editor_t* editor, int width, int height, int mouse_x, int mouse_y) {
	rlPushMatrix();
	rlTranslatef(0.0f, 24.0f, 0.0f);

	// adjust filter style for zoom
	bool pixel_perfect = (int)(editor->zoom * 100.0f) == (int)(editor->zoom * 100.0f);
	SetTextureFilter(editor->background_texture, (!pixel_perfect) ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);

	// Draw background
	DrawTexturePro(
		editor->background_texture,
		(Rectangle) { .width = width, .height = editor->background_texture.height },
		(Rectangle) { .width = width * editor->zoom, .height = editor->background_texture.height * editor->zoom },
		(Vector2) { 0 }, 0, WHITE
	);

	float gradient_height = 48;
	DrawRectangleGradientV(0, 512 - gradient_height, width, gradient_height, (Color) { 0 }, (Color) { .a = 128 });

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		editor->dragging = true;
		editor->drag_begin = (Vector2) { mouse_x, mouse_y };
	}
	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
		editor->drag_end = (Vector2) { mouse_x, mouse_y };
	}
	else if (editor->dragging) {
		strcpy(editor->footer_info, "");
		editor->dragging = false;
	}
	
	if (editor->dragging) {
		float tile_size = DEFAULT_TILE_SIZE;

		// X axis selection drag
		int x1 = (int)floorf(MIN(editor->drag_begin.x, editor->drag_end.x) / tile_size);
		int x2 = (int)ceilf(MAX(editor->drag_begin.x, editor->drag_end.x) / tile_size);

		// Y axis selection drag
		int y1 = (int)floorf(MIN(editor->drag_begin.y, editor->drag_end.y) / tile_size);
		int y2 = (int)ceilf(MAX(editor->drag_begin.y, editor->drag_end.y) / tile_size);

		// clamp to editor bounds
		struct {
			int* a;
			int* b;
			int begin, end;
		} clamp_regions[2] = {
			{ &x1, &x2, 0, 128 },
			{ &y1, &y2, 0, 32 }
		};
		for (int i = 0; i < 2; ++i) {
			int* vals[2] = { clamp_regions[i].a, clamp_regions[i].b };
			if (abs(*vals[1] - *vals[0]) == 0) {
				++(*vals[1]);
			}
			int begin = clamp_regions[i].begin, end = clamp_regions[i].end;
			for (int j = 0; j < 2; ++j) {
				*vals[j] = CLAMP(*vals[j], begin, end);
			}
		}

		// Drawing selection box
		Rectangle selection_rectangle = {
			.x = x1 * tile_size,
			.y = y1 * tile_size,
			.width = (x2 - x1) * tile_size,
			.height = (y2 - y1) * tile_size
		};

		int selection_breath = (64 * sinf(editor->selection_box_timer / 16.0f)) + 64;
		draw_window_selection(selection_rectangle, editor->selection_box_timer * 0.75f, 2, (Color) { 255, 255, 255, 128 + selection_breath }, (Color) { 0, 0, 0, 128 + selection_breath });
	
		// Editor selection text in footer
		strcpy(editor->footer_info, TextFormat("Selection: %ix%i", y2-y1, x2-x1));
	}

	rlPopMatrix();
}

void editor_run(editor_t* editor) {
	// core window space zoom
	if (IsKeyDown(KEY_LEFT_CONTROL)) {
		float last_zoom = editor->zoom;
		if (IsKeyPressed(KEY_ZERO)) {
			editor->zoom = 1.0f;
		}
		else {
			editor->zoom += GetMouseWheelMove() / 10.0f;
			editor->zoom = CLAMP(editor->zoom, 1, 4);
		}
		if (editor->zoom != last_zoom) {
			strcpy(editor->footer_info, TextFormat("Zoom: %i%%", (int)(editor->zoom * 100.0f)));
		}
	}

	int w = GetScreenWidth(), h = GetScreenHeight();
	Vector2 mouse = GetMousePosition();

	// toolbar
	const char* labels[4] = { "File", "Edit", "View", "Options" };
	editor_toolbar(editor, w, WINDOW_STATUSBAR_HEIGHT, labels, 4);

	// body
	editor_viewport(editor, w, h - 24, mouse.x, mouse.y - 24);

	// footer
	GuiStatusBar((Rectangle) { 0, h - 24, 200, 24 }, "Tile Mode");
	GuiStatusBar((Rectangle) { 200, h - 24, w - 200, 24 }, editor->footer_info);

	window_run(&editor->tiles_window.base, GetFrameTime());

	editor->selection_box_timer += GetFrameTime() * 60.0f;
}

#endif
#endif
#endif
#pragma endregion

#pragma region Array List

#define ARRAYLIST_DEFINE(type, type_name) \
typedef struct type_name { type* data; int count; int _capacity; } type_name##_arraylist_t; \
void type_name##_arraylist_init(type_name##_arraylist_t* list, int initial_capacity) { \
    list->count = 0; \
    list->_capacity = initial_capacity; \
    list->data = malloc(list->_capacity * sizeof(type)); \
} \
void type_name##_arraylist_free(type_name##_arraylist_t* list) { \
    list->count = list->_capacity = 0; \
    free(list->data); \
} \
void type_name##_arraylist_push(type_name##_arraylist_t* list, type data) { \
    if (list->count >= list->_capacity) { \
        list->_capacity *= ARRAYLIST_SCALE_FACTOR; \
        list->data = realloc(list->data, list->_capacity * sizeof(type)); \
    } \
    list->data[list->count] = data; \
    list->count++; \
} \
type type_name##_arraylist_get(type_name##_arraylist_t* list, int index) { \
    return (index < 0 || index >= list->count) ? (type) { 0 } : list->data[index]; \
} \
void type_name##_arraylist_remove(type_name##_arraylist_t* list, int index) { \
    if (index < 0 || index >= list->count) return; \
    for (int i = index; i < list->count - 1; i++) list->data[i] = list->data[i + 1]; /* pushes data to the left */ \
    --list->count; \
} \

#pragma endregion

#pragma region Tilemap

typedef enum collision_type {
	COLLISION_AIR = 0,
	COLLISION_PLATFORM,
	COLLISION_SOLID
} collision_type_t;

typedef struct tile {
	collision_type_t collision;
} tile_t;

typedef struct tilemap {
	tile_t** data;
	int width;
	int height;
	int tile_size;
} tilemap_t;

void tilemap_init(tilemap_t* map, int width, int height, int tile_size) {
	*map = (tilemap_t) {
		.width = width,
		.height = height,
		.tile_size = tile_size,
		.data = malloc(width * sizeof(tilemap_t*))
	};
	for (int x = 0; x < width; ++x) {
		map->data[x] = calloc(height, sizeof(tilemap_t));
	}
}

void tilemap_free(tilemap_t* map) {
	for (int x = 0; x < map->width; ++x) {
		free(map->data[x]);
	}
	free(map->data);
}

tile_t tilemap_get(const tilemap_t* map, int x, int y) {
	if (x < 0 || y < 0 || x >= map->width || y >= map->height) {
		return (tile_t) { .collision = COLLISION_AIR };
	}
	return map->data[x][y];
}

Rectangle tilemap_get_rectangle(const tilemap_t* map, int x, int y) {
	if (x < 0 || y < 0 || x >= map->width || y >= map->height) return (Rectangle) { 0, 0, 0, 0 };
	return (Rectangle) { x * (float)map->tile_size, y * (float)map->tile_size, (float)map->tile_size, (float)map->tile_size };
}

void tilemap_set(tilemap_t* map, int x, int y, tile_t val) {
	if (x < 0 || y < 0 || x >= map->width || y >= map->height) {
		return;
	}
	map->data[x][y] = val;
}

#pragma endregion

#pragma region Control States

typedef struct controller_buttons {
	int h, v;
	bool a, b, x, y;
	bool l, r;
} controller_buttons_t;

struct controller_state {
	controller_buttons_t current;
	controller_buttons_t previous;
};

void controller_state_update(controller_state_t* state) {
	memcpy(&state->previous, &state->current, sizeof(controller_state_t));
	state->current = (controller_buttons_t) {
		.a = IsKeyDown(KEY_X),
		.b = IsKeyDown(KEY_Z),
		.x = IsKeyDown(KEY_C),
		.y = IsKeyDown(KEY_V),
		.h = IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT),
		.v = IsKeyDown(KEY_DOWN) - IsKeyDown(KEY_UP)
	};
}

#pragma endregion

#pragma region Render Context

struct render_context {
	Texture sprite_atlas;
	RenderTexture render_texture;
};

#pragma endregion

#pragma region Backgrounds

typedef struct background {
	float x, y;
	float parallax_x;
	float parallax_y;
	Texture tex;
	bool clamp_x;
	bool clamp_y;
} background_t;

/**
 * Initializes a background
 * @param res_loc		Local background resource name, ommitting path and file type (i.e. "glade" for "glade.png" inside of the backgrounds folder)
 * @param background	Background to initialize
 * @param tiled			Wrap texture at the ends
 */
void background_init(const char* res_loc, background_t* background, bool tiled) {
	*background = (background_t) { 0 };

	// index path
	char indexed_loc[MAX_PATH_LEN] = "";
	path_index(res_loc, indexed_loc);

	// get full bg path
	char img_path[MAX_PATH_LEN] = "";
	snprintf(img_path, sizeof img_path, BACKGROUNDS_PATH "/%s.png", indexed_loc);

	// open file for reading
	FILE* f = fopen(img_path, "r");
	if (f == NULL) {
		printd("Background [%s] not found\n", res_loc);
		return;
	}

	printd("Loading background res [%s]\n", img_path);
	background->tex = LoadTexture(img_path);
	
	if (tiled) {
		SetTextureWrap(background->tex, TEXTURE_WRAP_REPEAT);
	}

	fclose(f);
}

/**
 * Draw background to screen using position / offset / parallax data in background
 * @param background Pointer to background to draw
 */
void background_draw(background_t* background) {
	int bg_x = (int)(background->x * background->parallax_x) % background->tex.width;

	Rectangle bg_rect = (Rectangle) { .width = background->tex.width, .height = background->tex.height };
	Rectangle screen_rect = (Rectangle) { bg_x, background->y * background->parallax_y, bg_rect.width, bg_rect.height };
	DrawTexturePro(background->tex, bg_rect, screen_rect, (Vector2) { 0, 0 }, 0, WHITE);
}

/**
 * Free data inside of a background
 * @param background Pointer to background to free data from
 */
void background_free(background_t* background) {
	UnloadTexture(background->tex);
}

#pragma endregion

#pragma region Sprites

struct sprite_frame {
	int x, y;
};

typedef struct sprite {
	int width, height;
	int frame_count;
	sprite_frame_t* frames;
	int order_count;
	int* order;
} sprite_t;

/**
 * Frees the data inside of a sprite
 * @param sprite Pointer to sprite to free data from
 */
void sprite_free(sprite_t* sprite) {
	if (sprite->frames) {
		free(sprite->frames);
	}
	if (sprite->order) {
		free(sprite->order);
	}
}

const sprite_frame_t sprite_get_frame(const sprite_t* sprite, int image_index) {
	return (sprite->order == NULL) ?
		sprite->frames[image_index % sprite->frame_count] :					// no custom order; get frame from image index
		sprite->frames[sprite->order[image_index % sprite->order_count]];	// custom order; get frame from order index
}

void sprite_draw_ex(sprite_t* sprite, int image_index, float x, float y, int origin_x, int origin_y, bool flip_x, bool flip_y, render_context_t* context) {
	// frame to utilize from sprite frames
	sprite_frame_t frame = sprite_get_frame(sprite, image_index);
	Rectangle sprite_rect = (Rectangle) { floorf(frame.x), floorf(frame.y), sprite->width, sprite->height };
	if (flip_x) {
		sprite_rect.width = -sprite_rect.width;
	}
	Rectangle screen_rect = (Rectangle) { floorf(x), floorf(y), sprite_rect.width, sprite_rect.height };
	DrawTexturePro(context->sprite_atlas, sprite_rect, screen_rect, (Vector2) { origin_x, origin_y }, 0, WHITE);
}

void sprite_draw(sprite_t* sprite, int image_index, float x, float y, bool flip_x, bool flip_y, render_context_t* context) {
	sprite_draw_ex(sprite, image_index, x, y, 0.0f, 0.0f, flip_x, flip_y, context);
}

void sprite_init(const char* res_loc, sprite_t* sprite, Image* atlas_img, stbrp_context* rect_packer) {
	// replace all instances of "." with "/" for local resources
	char indexed_fname[MAX_PATH_LEN] = "";
	path_index(res_loc, indexed_fname);

	*sprite = (sprite_t) { 0 };
    
    char image_path[MAX_PATH_LEN] = "", data_path[MAX_PATH_LEN] = "";
	snprintf(image_path, sizeof image_path, SPRITES_PATH "/%s.png", indexed_fname);
	snprintf(data_path, sizeof data_path, SPRITES_PATH "/%s.dat", indexed_fname);

	FILE* file;
	if ((file = fopen(image_path, "r")) == NULL) {
		return;
	}
	fclose(file);

    Image img = LoadImage(image_path);
	if (img.data == NULL) {
		return;
	}
    int sprite_width = img.width, sprite_height = img.height;

    // load .dat file and initialize the animation frames and order
	if ((file = fopen(data_path, "r")) == NULL) {
		sprite->frame_count = ceilf(sprite_height / (float)sprite_width);
		sprite->frames = malloc(sprite->frame_count * sizeof(sprite_frame_t));
		int frame_height = sprite_width > sprite_height ? sprite_height : sprite_width;
		sprite->width = sprite_width;
		sprite->height = frame_height;
		for (int i = 0; i < sprite->frame_count; ++i) {
			stbrp_rect r = { 0, sprite_width, frame_height };
			if (stbrp_pack_rects(rect_packer, &r, 1)) {
				ImageDraw(atlas_img, img, (Rectangle) { 0, (i * frame_height), sprite_width, frame_height }, (Rectangle) { r.x, r.y, sprite_width, frame_height }, WHITE);
				sprite->frames[i].x = r.x;
				sprite->frames[i].y = r.y;
			}
		}
		UnloadImage(img);
		printd("Loaded sprite [%s] with [%d] frames\n", res_loc, sprite->frame_count);
		return;
	}
    
    char order_buf[256] = "";
    char line[256] = "";

    while (fgets(line, sizeof line, file)) {
        if (sprite->frame_count == 0 && strncmp(line, "frames:", 7) == 0) {
            if (sscanf(line, "frames: %d", &sprite->frame_count) != 1) {
                goto close_file;
            }
			else {
                sprite->frames = malloc(sprite->frame_count * sizeof(sprite_frame_t));
                sprite_height = img.height / sprite->frame_count;
                printd("Frame dimensions: [%d, %d]\n", sprite_width, sprite_height);

				sprite->width = sprite_width;
				sprite->height = sprite_height;
				for (int i = 0; i < sprite->frame_count; ++i) {
					stbrp_rect r = (stbrp_rect) { .w = sprite_width, .h = sprite_height };
					if (stbrp_pack_rects(rect_packer, &r, 1)) {
						ImageDraw(atlas_img, img, (Rectangle) { 0, (int)(i * sprite_height), sprite_width, sprite_height }, (Rectangle) { r.x, r.y, sprite_width, sprite_height }, WHITE);
						sprite->frames[i].x = r.x;
						sprite->frames[i].y = r.y;
					}
				}
            }
        }
        if (sprite->order == NULL && strncmp(line, "order:", 6) == 0) {
            if (sscanf(line, "order: %s", order_buf) != 1) {
                goto close_file;
            }
			else {
                size_t order_len = strlen(order_buf);

                // fetch count of frames to allocate for order
                for (int i = 0; i < order_len;) {
                    int temp;
                    if (sscanf(order_buf + i, "%d", &temp) == 1) {
                        while (order_buf[i] != ',' && i < order_len) {
                            i++;
                        }
                        while (order_buf[i] == ',' && i < order_len) {
                            i++;
                        }
                        sprite->order_count++;
                    }
					else i++;
                }

                sprite->order = malloc(sprite->order_count * sizeof(int));

                // put animation frame order into the animation order array
                for (int i = 0, num_count = 0; i < order_len;) {
                    int temp;
                    if (sscanf(order_buf + i, "%d", &temp) == 1) {
                        while (order_buf[i] != ',' && i < order_len) {
                            i++;
                        }
                        while (order_buf[i] == ',' && i < order_len) {
                            i++;
                        }
                        sprite->order[num_count++] = temp;
                    }
					else i++;
                }
            }
        }
    }

close_file:
#ifdef DEV
	if (sprite->order) {
		printd("Loaded sprite [%s] with [%d] frames and pre-defined order of [", res_loc, sprite->frame_count);
		for (int i = 0; i < sprite->order_count; ++i) {
			printd("%d", sprite->order[i]);
			if (i < sprite->order_count - 1) {
				printd(", ");
			}
		}
		printd("]\n");
	}
	else {
		printd("Loaded sprite [%s] with [%d] frames\n", res_loc, sprite->frame_count);
	}
#endif
    fclose(file);
}

ARRAYLIST_DEFINE(sprite_t*, spriteptr)

#pragma endregion

#pragma region Sounds // TODO: should be passed by context

struct sounds {
	Sound bump;
	Sound jump;
} sounds;

#pragma endregion

#pragma region Text

typedef struct font {
	sprite_t sprite_data;
	char order[256];
	int spacing;
} font_t;

font_t fnt_hud;

/**
 * Initializes a font
 * @param res_loc		Resource location
 * @param order			Order of the font (e.g. 'ABCDEFGHIJKLMNOPQRSTUVWXYZ')
 * @param font			Pointer to the font to initialize
 * @param atlas			Pointer to the atlas to add font sprites to
 * @param rect_packer	Pointer to current atlas packer
 */
void font_init(const char* res_loc, const char* order, font_t* font, Image* atlas, stbrp_context* rect_packer) {
	*font = (font_t) { 0 };
	sprite_init(res_loc, &font->sprite_data, atlas, rect_packer);
	int order_len = strlen(order);
	memcpy((void*)font->order, order, (size_t)order_len + 1);
#ifdef DEV
	printd("Font [%s] order: [", res_loc);
	for (int i = 0; i < order_len; ++i) {
		printd("%c", font->order[i]);
		if (i < order_len - 1) {
			printd(", ");
		}
	}
	printd("]\n");
#endif
}

/**
 * Frees a fonts data
 * @param font Font to free internal data from
 */
void font_free(font_t* font) {
	sprite_free(&font->sprite_data);
}

/**
 * Draws text to the screen at a given position
 * @param text 		Text to draw
 * @param font 		Font to draw with
 * @param x			X position to draw to
 * @param y			Y position to draw to
 * @param context	Current rendering context
 */
void text_draw(const char* text, font_t* font, float x, float y, render_context_t* context) {
	size_t string_len = strlen(text), order_len = strlen(font->order);
	int _x = x, _y = y;
	int font_width = font->sprite_data.width, font_height = font->sprite_data.height;
	for (int i = 0; i < string_len; ++i) {
		if (text[i] == ' ') {
			_x += font->spacing + font_width;
			continue;
		}
		if (text[i] == '\n') {
			_x = 0;
			_y += font_height;
			continue;
		}
		for (int j = 0; j < order_len; ++j) {
			if (font->order[j] == text[i]) {
				sprite_draw(&font->sprite_data, j, _x, _y, false, false, context);
				_x += font_width + font->spacing;
				break;
			}
		}
	}
}

#pragma endregion

#pragma region Animations

typedef enum powerup { POWERUP_SMALL = 0, POWERUP_BIG, POWERUP_FIRE } powerup_t;

struct mario_sprites {
	sprite_t idle[2];
	sprite_t walk[2];
	sprite_t run[2];
	sprite_t skid[2];
	sprite_t kick[2];
	sprite_t jump[2];
	sprite_t runjump[2];
	sprite_t spin[2];
	sprite_t crouch[2];
	sprite_t hold_idle[2];
	sprite_t hold_walk[2];
	sprite_t hold_swim[2];
	sprite_t swim[2];
	sprite_t peace[2];
	sprite_t ride[2];
	sprite_t slide[2];
} mario_sprites;

#pragma endregion

#pragma region Physics & Collision

struct physics_body {
	float x, y;
	int width, height;
	float origin_x, origin_y;
	float xspd, yspd;
	float xspd_max, yspd_max;
	float grav;
	bool grounded;
};

/**
 * Initializes a physics body with default values
 * @param body		Physics body to initialize
 * @param width		Width of the physics body
 * @param height	Height of the physics body
 */
void physics_body_init(physics_body_t* body, int width, int height) {
	*body = (physics_body_t) {
		.width = width,
		.height = height,
		.origin_x = 0.5f,
		.origin_y = 1.0f,
		.xspd_max = 5.0f,
		.yspd_max = 5.0f,
		.grav = 0.2f,
	};
}

/**
 * Resolves x-axis collisions on a physics body given a tilemap
 * @param body 	Physics body to perform collisions on
 * @return Rectangle that contains world space collision bounds
 */
Rectangle physics_body_get_rectangle(const physics_body_t* body) {
	return (Rectangle) {
		.x = body->x - body->width * body->origin_x,
		.y = body->y - (body->height * body->origin_y),
		.width = body->width,
		.height = body->height
	};
}

/**
 * Resolves x-axis collisions on a physics body given a tilemap
 * @param body	Physics body to perform collisions on
 * @param map	Tilemap to check for collisions from
 */
void resolve_collisions_x(physics_body_t* body, const tilemap_t* map) {
	int tile_size = map->tile_size;

	Rectangle body_rect = physics_body_get_rectangle(body);
	Rectangle left_rect = { body_rect.x, body_rect.y, 0, body_rect.height };
	Rectangle right_rect = { body_rect.x + body_rect.width, body_rect.y, 0, body_rect.height };

	int left = body_rect.x / tile_size, right = (body_rect.x + body_rect.width) / tile_size;
	int top = body_rect.y / tile_size, bottom = (body_rect.y + body_rect.height) / tile_size;

	for (int y = top - 1; y <= bottom + 1; ++y) {
		// left
		for (int x = left - 1; x <= left; ++x) {
			collision_type_t tile_type = tilemap_get(map, x, y).collision;
			if (tile_type == COLLISION_SOLID) {
				Rectangle tile_rect = tilemap_get_rectangle(map, x, y);
				if (rectangle_collision(left_rect, tile_rect)) {
					body->x = tile_rect.x + tile_size + (body->width * body->origin_x);
					body->xspd = 0;
				}
			}
		}
		// right
		for (int x = right; x <= right + 1; ++x) {
			collision_type_t tile_type = tilemap_get(map, x, y).collision;
			if (tile_type == COLLISION_SOLID) {
				Rectangle tile_rect = tilemap_get_rectangle(map, x, y);
				if (rectangle_collision(right_rect, tile_rect)) {
					body->x = tile_rect.x - body_rect.width + (body->width * body->origin_x);
					body->xspd = 0;
				}
			}
		}
	}
}

/**
 * Resolves y-axis collisions on a physics body given a tilemap
 * @param body 	Physics body to perform collisions on
 * @param map	Tilemap to check for collisions from
 */
void resolve_collisions_y(physics_body_t* body, const tilemap_t* map) {
	body->grounded = false;
	int tile_size = map->tile_size;

	Rectangle body_rect = physics_body_get_rectangle(body);
	Rectangle bottom_rect = { body_rect.x, body_rect.y + body_rect.height, body_rect.width, 0 };
	Rectangle top_rect = { body_rect.x, body_rect.y, body_rect.width, 0 };

	int left = body_rect.x / tile_size, right = (body_rect.x + body_rect.width) / tile_size;
	int top = body_rect.y / tile_size, bottom = (body_rect.y + body_rect.height) / tile_size;

	for (int x = left - 1; x <= right + 1; ++x) {
		// up
		for (int y = top - 1; y <= top; ++y) {
			collision_type_t tile_type = tilemap_get(map, x, y).collision;
			if (tile_type == COLLISION_SOLID) {
				Rectangle tile_rect = tilemap_get_rectangle(map, x, y);
				if (rectangle_collision(top_rect, tile_rect)) {
					body->y = tile_rect.y + tile_size + (body->height * body->origin_y);
					body->yspd = 0;
					PlaySound(sounds.bump);
					return;
				}
			}
		}
		// down
		for (int y = bottom; y <= bottom + 1; ++y) {
			// horizontal line on bottom of physbody
			collision_type_t tile_type = tilemap_get(map, x, y).collision;
			if (tile_type == COLLISION_SOLID || tile_type == COLLISION_PLATFORM) {
				Rectangle tile_rect = tilemap_get_rectangle(map, x, y);
				if (rectangle_collision(bottom_rect, tile_rect)) {
					body->y = tile_rect.y;
					body->yspd = 0;
					body->grounded = true;
					return;
				}
			}
		}
	}
}

void physics_body_update(physics_body_t* body, const tilemap_t* tilemap) {
	// adjust speeds
	body->yspd += body->grav;
	body->yspd = MIN(body->yspd, body->yspd_max);
	if (body->grounded) body->xspd = CLAMP(body->xspd, -body->xspd_max, body->xspd_max);
	
	// move & collide
	body->x += body->xspd;
	resolve_collisions_x(body, tilemap);
	body->y += body->yspd;
	resolve_collisions_y(body, tilemap);
}

#pragma endregion

#pragma region Entity

typedef long long entity_id_t;

typedef enum entity_type {
	ENTITY_NONE = -1,
	ENTITY_GOOMBA = 0,
	ENTITY_KOOPA,
	ENTITY_PIRANHA,
	ENTITY_COUNT,
} entity_type_t;

struct entity {
	entity_id_t id;
	entity_type_t type;
	physics_body_t body;
	bool is_active;
	void (*update)(struct entity*, struct level*);
	void (*draw)(struct entity*, struct level*, render_context_t* context);
};

ARRAYLIST_DEFINE(entity_t*, entityptr)

#pragma endregion

#pragma region Player Struct

struct player {
	physics_body_t body;
	sprite_t* sprites_index;
	bool flip_x;
	bool is_big;
	bool is_crouching;
	float image_index;
};

#pragma endregion

#pragma region Level Struct, Init, & Free

typedef struct camera {
	int x, y;
	int offset_x, offset_y;
	int width, height;
} camera_t;

struct level {
	player_t player;
	entityptr_arraylist_t entities;
	Color background_color;
	background_t background;
	tilemap_t tilemap;
	entity_id_t next_entity_id;
	camera_t camera;
};

void level_init(level_t* level, const char* background_res, Color background_color, int width_in_tiles, int height_in_tiles, int tile_size) {
	*level = (level_t) { 
		.background_color = background_color,
		.camera = (camera_t) {
			.offset_x = (-GAME_WIDTH / 2.0f),
			.offset_y = (-GAME_HEIGHT / 2.0f),
			.width = GAME_WIDTH,
			.height = GAME_HEIGHT
		}
	};

	// entities
	player_init(&level->player);
	entityptr_arraylist_init(&level->entities, ENTITY_DEFAULT_ALLOCATION_SIZE);
	
	// bg
	background_init(background_res, &level->background, true);
	level->background.y = -level->background.tex.height + GAME_HEIGHT;
	level->background.clamp_x = false;
	level->background.clamp_y = true;
	level->background.parallax_x = 0.5f;
	level->background.parallax_y = 1.0f;

	// tilemap (temporary. delegated to a file type eventually)
	tilemap_init(&level->tilemap, width_in_tiles, height_in_tiles, tile_size);
	for (int i = 0; i <= 7; ++i) {
		tilemap_set(&level->tilemap, i, 14, (tile_t) { .collision = COLLISION_SOLID });
	}
	for (int i = 8; i <= 12; ++i) {
		tilemap_set(&level->tilemap, i, 15, (tile_t) { .collision = COLLISION_SOLID });
	}
	for (int i = 13; i <= 14; ++i) {
		tilemap_set(&level->tilemap, i, 14, (tile_t) { .collision = COLLISION_SOLID });
	}
	tilemap_set(&level->tilemap, 15, 13, (tile_t) { .collision = COLLISION_SOLID });
	tilemap_set(&level->tilemap, 15, 12, (tile_t) { .collision = COLLISION_SOLID });
	for (int i = 17; i <= 19; ++i) {
		tilemap_set(&level->tilemap, i, 11, (tile_t) { .collision = COLLISION_SOLID });
	}
	tilemap_set(&level->tilemap, 16, 12, (tile_t) { .collision = COLLISION_SOLID });
	for (int i = 20; i <= level->tilemap.width; ++i) {
		tilemap_set(&level->tilemap, i, 10, (tile_t) { .collision = COLLISION_SOLID });
	}

	tilemap_set(&level->tilemap, 0, 10, (tile_t) { .collision = COLLISION_SOLID });
	for (int i = 2; i <= 5; ++i) {
		tilemap_set(&level->tilemap, i, 9, (tile_t) { .collision = COLLISION_SOLID });
	}
	for (int i = 6; i <= 10; ++i) {
		tilemap_set(&level->tilemap, i, 8, (tile_t) { .collision = COLLISION_SOLID });
	}
	for (int i = 5; i <= 7; ++i) {
		tilemap_set(&level->tilemap, 10, i, (tile_t) { .collision = COLLISION_SOLID });
	}
}

void level_free(level_t* level) {
	for (int i = 0; i < level->entities.count; ++i) {
		entity_t* entity = entityptr_arraylist_get(&level->entities, i);
		if (entity != NULL) {
			free(entity);
		}
	}
	entityptr_arraylist_free(&level->entities);
	background_free(&level->background);
	tilemap_free(&level->tilemap);
}

#pragma endregion

#pragma region Entity Functions

void entity_init(entity_t* entity, entity_type_t type, level_t* level, int width, int height) {
	*entity = (entity_t) {
		.id = level->next_entity_id++,
		.type = type
	};
	physics_body_init(&entity->body, width, height);
}

void entity_update(entity_t* entity, level_t* level) {
	// foobuh barfew lorem ipsum sum checksum ugugghh
}

int compare_entities(const void* a, const void* b) {
	const entity_t* entity_a = (const entity_t*)a;
	const entity_t* entity_b = (const entity_t*)b;
	return entity_a->id - entity_b->id; // returns 0 if the ID matches
}

#pragma endregion

#pragma region Enemies

typedef struct entity_goomba {
	entity_t base;
} entity_goomba_t;

void goomba_init(entity_goomba_t* entity, level_t* level) {
	*entity = (entity_goomba_t) { 0 };
	entity_init(&entity->base, ENTITY_GOOMBA, level, 8, 6);
}

void goomba_update(entity_t* entity, level_t* level) {
	entity_goomba_t* g = (entity_goomba_t*)entity;
}

void goomba_draw(entity_t* entity, level_t* level, render_context_t* context) {
	entity_goomba_t* g = (entity_goomba_t*)entity;
}

#pragma endregion

#pragma region Game Control

struct game {
	int controller_count;
	render_context_t render_context;
	controller_state_t* controllers;
	level_t* level;
};

void game_update(game_t* game) {
	for (int i = 0; i < game->controller_count; ++i) {
		controller_state_update(&game->controllers[i]);
	}
	if (game->level) {
		level_update(game->level, game);
	}
}

void game_draw(game_t* game) {
	if (game->level) {
		level_draw(game->level, &game->render_context);
	}

	// MVP - model, view [ view * model ]
	text_draw("HELLO WORLD", &fnt_hud, 0, 0, &game->render_context);
}

void game_init(const char* window_title, game_t* game) {
	// set all game values to 0 (nullified)
	*game = (game_t) { 0 };

	// rng
	srand(0);

	// start window
	SetTraceLogLevel(LOG_NONE);
#ifdef EDIT_MODE
#endif
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(256, 224, window_title);
	InitAudioDevice();
#ifdef EDIT_MODE
	SetTargetFPS(120);
#else
	SetTargetFPS(60);
#endif

	Image icon = LoadImage("assets/icon_editor.png");
	SetWindowIcon(icon);
	UnloadImage(icon);

	// atlas image
	// todo: asset loading automation!
	{
		Image atlas_img = GenImageColor(TEXTURE_ATLAS_WIDTH, TEXTURE_ATLAS_HEIGHT, (Color) { 255, 255, 255, 0 });

		stbrp_context rect_packer;
		stbrp_node* nodes = malloc(sizeof(stbrp_node) * MAX_TEXTURE_NODES);
		stbrp_init_target(&rect_packer, TEXTURE_ATLAS_WIDTH, TEXTURE_ATLAS_HEIGHT, nodes, MAX_TEXTURE_NODES);

		sprite_init("mario.idle_small",	&mario_sprites.idle[POWERUP_SMALL],	&atlas_img, &rect_packer);
		sprite_init("mario.walk_small",	&mario_sprites.walk[POWERUP_SMALL],	&atlas_img, &rect_packer);
		sprite_init("mario.run_small", 	&mario_sprites.run[POWERUP_SMALL], 	&atlas_img, &rect_packer);
		sprite_init("mario.jump_small",	&mario_sprites.jump[POWERUP_SMALL],	&atlas_img, &rect_packer);
		sprite_init("mario.skid_small",	&mario_sprites.skid[POWERUP_SMALL],	&atlas_img, &rect_packer);

		sprite_init("mario.idle_big", 	&mario_sprites.idle[POWERUP_BIG], 	&atlas_img, &rect_packer);
		sprite_init("mario.crouch_big", &mario_sprites.crouch[POWERUP_BIG], &atlas_img, &rect_packer);
		sprite_init("mario.walk_big", 	&mario_sprites.walk[POWERUP_BIG], 	&atlas_img, &rect_packer);
		sprite_init("mario.run_big", 	&mario_sprites.run[POWERUP_BIG], 	&atlas_img, &rect_packer);
		sprite_init("mario.jump_big", 	&mario_sprites.jump[POWERUP_BIG], 	&atlas_img, &rect_packer);
		sprite_init("mario.skid_big", 	&mario_sprites.skid[POWERUP_BIG], 	&atlas_img, &rect_packer);

		font_init("font.hud", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.,*-!@|=:", &fnt_hud, &atlas_img, &rect_packer);
		fnt_hud.spacing = 0;

		free(nodes);

		game->render_context.sprite_atlas = LoadTextureFromImage(atlas_img);

		ExportImage(atlas_img, "sprite_atlas_dump.png");
		UnloadImage(atlas_img);
	}
	{
		Image atlas_img = GenImageColor(TILE_ATLAS_WIDTH, TILE_ATLAS_HEIGHT, (Color) { 255, 255, 255, 0 });

		stbrp_context rect_packer;
		stbrp_node* nodes = malloc(sizeof(stbrp_node) * MAX_TEXTURE_NODES);
		stbrp_init_target(&rect_packer, TILE_ATLAS_WIDTH, TILE_ATLAS_HEIGHT, nodes, MAX_TEXTURE_NODES);

		const char* tilesets[2] = { "glade", "ice" };
		const int tile_size = DEFAULT_TILE_SIZE;
		for (int i = 0; i < 2; ++i) {
			Image tls = LoadImage(TextFormat("assets/tiles/%s.png", tilesets[i]));
			for (int i = 0; i < tls.width / tile_size; ++i) {
				for (int j = 0; j < tls.height / tile_size; ++j) {
					bool has_img_data = false;
					for (int x = 0; x < tile_size; ++x) {
						for (int y = 0; y < tile_size; ++y) {
							Color c_at = GetImageColor(tls, (i * tile_size) + x, (j * tile_size) + y);
							if (c_at.a != 0) {
								has_img_data = true;
							}
						}
					}
					if (!has_img_data) {
						continue;
					}
					stbrp_rect r = { .w = tile_size, .h = tile_size };
					if (stbrp_pack_rects(&rect_packer, &r, 1)) {
						ImageDraw(&atlas_img, tls, (Rectangle) { (i * tile_size), (j * tile_size), tile_size, tile_size }, (Rectangle) { r.x, r.y, tile_size, tile_size }, WHITE);
					}
				}
			}
		}

		free(nodes);

		ExportImage(atlas_img, "tile_atlas_dump.png");
		UnloadImage(atlas_img);
	}

#ifdef EDIT_MODE
	GuiLoadStyleDark();
	editor_t editor;
	editor_init(&editor);
	while (!WindowShouldClose()) {
		BeginDrawing();
		ClearBackground(BLACK);
		editor_run(&editor);
		EndDrawing();
	}
#else
	sounds = (struct sounds) {
		.bump = LoadSound("assets/sounds/bump.wav"),
		.jump = LoadSound("assets/sounds/jump.wav")
	};

	// initialization would be something like this for an entity:
	entity_goomba_t goomba = (entity_goomba_t){
		.base = {
			.id = 1,
			.type = ENTITY_GOOMBA,
			.update = goomba_update,
			.draw = goomba_draw
		}
	};

	// controller set-up
	game->controller_count = 1;
	game->controllers = malloc(MAX_CONTROLLERS * sizeof(controller_state_t));

	// first level init
	game->level = malloc(sizeof(level_t));
	level_init(game->level, "overworld", BLACK_SKY, 48, 16, DEFAULT_TILE_SIZE);

	// create rendering surface 
	game->render_context.render_texture = LoadRenderTexture(GAME_WIDTH, GAME_HEIGHT);
	RenderTexture hud_texture = LoadRenderTexture(GAME_WIDTH, GAME_HEIGHT);

	while (!WindowShouldClose()) {
		// update
		game_update(game);

		// draw game to render target
		BeginTextureMode(game->render_context.render_texture);
		if (game->level != NULL) {
			ClearBackground(game->level->background_color);
		}
		game_draw(game);
		EndTextureMode();

		BeginTextureMode(hud_texture);
		ClearBackground((Color) { 0 });
		EndTextureMode();

		// draw render target to screen
		BeginDrawing();
		ClearBackground(BLACK);

		int window_width = GetScreenWidth();
		int window_height = GetScreenHeight();

		int render_width = game->render_context.render_texture.texture.width;
		int render_height = game->render_context.render_texture.texture.height;

		int res_scale = MIN(window_width / render_width, window_height / render_height);

		Rectangle source_area = { 0, 0, render_width, -render_height };
		Rectangle dest_area = { (int)((window_width / 2.0f) - (render_width * res_scale / 2.0f)), (int)((window_height / 2.0f) - (render_height * res_scale / 2.0f)), render_width * res_scale, -render_height * res_scale };

		DrawTexturePro(game->render_context.render_texture.texture, source_area, dest_area, (Vector2) { 0, 0 }, 0, WHITE);
		
		EndDrawing();
	}

	UnloadRenderTexture(game->render_context.render_texture);
	UnloadRenderTexture(hud_texture);
#endif
}

void game_end(game_t* game) {
	UnloadTexture(game->render_context.sprite_atlas);

	if (game->controllers != NULL) {
		free(game->controllers);
	}

	if (game->level != NULL) {
		level_free(game->level);
		free(game->level);
	}

	CloseWindow();
}

#pragma endregion

#pragma region Level Update & Draw

void level_update_entities(level_t* level) {
	for (int i = 0; i < level->entities.count; ++i) {
		entity_t* e = entityptr_arraylist_get(&level->entities, i);
		if (e != NULL) {
			entity_update(e, level);	// all entity update
			if (e->update != NULL) {	// uniquely assigned update
				e->update(e, level);
			}
		}
	}
}

void camera_set_position(camera_t* camera, int x, int y, const tilemap_t* tilemap_bounds) {
	camera->x = CLAMP(x + camera->offset_x, 0, (tilemap_bounds->width * tilemap_bounds->tile_size) - camera->width);
	camera->y = CLAMP(y + camera->offset_y, 0, (tilemap_bounds->height * tilemap_bounds->tile_size) - camera->height);
}

void level_update(level_t* level, game_t* game) {
	player_update(&level->player, level, &game->controllers[0]);
	level_update_entities(level);
	camera_set_position(&level->camera, level->player.body.x, level->player.body.y, &level->tilemap);
	level->background.x = -level->camera.x;
	level->background.y = ((float)(-level->camera.y) / 2.0f) - 128;
}

void level_draw(level_t* level, render_context_t* context) {
	background_draw(&level->background);

	// localize camera space coordinates to local coordinates by offsetting the current model matrix
	rlPushMatrix();
	rlTranslatef(-level->camera.x, -level->camera.y, 0);

	int tile_size = level->tilemap.tile_size;
	int cam_tile_x1 = level->camera.x / level->tilemap.tile_size, cam_tile_x2 = (level->camera.x + GAME_WIDTH) / (float)tile_size;
	int cam_tile_y1 = level->camera.y / level->tilemap.tile_size, cam_tile_y2 = (level->camera.y + GAME_HEIGHT) / (float)tile_size;

	for (int it = 0; it < 2; ++it) {
		if (it == 1) {
			BeginBlendMode(BLEND_ADDITIVE);
		}
		for (int i = cam_tile_x1; i <= cam_tile_x2; ++i) {
			for (int j = cam_tile_y1; j <= cam_tile_y2; ++j) {
				collision_type_t tile_type = tilemap_get(&level->tilemap, i, j).collision;
				if (tile_type != COLLISION_AIR) {
					float alpha = 1.0f - CLAMP(distance(level->player.body.x, level->player.body.y, (i * tile_size) + (tile_size / 2.0f), (j * tile_size) + (tile_size / 2.0f)) / 64.0f, 0.0f, 1.0f);
					if (it == 0) {
						DrawRectangleLinesEx(tilemap_get_rectangle(&level->tilemap, i, j), 1, (Color) { 0, 200, 255, (const char)((alpha * 128.0f)) });
					}
					else {
						DrawRectangleRec(tilemap_get_rectangle(&level->tilemap, i, j), (Color) { 0, 128, 255, (const char)((alpha * 128.0f)) });
					}
				}
			}
		}
		if (it == 1) {
			EndBlendMode();
		}
	}

	for (int i = 0; i < level->entities.count; ++i) {
		entity_t* e = entityptr_arraylist_get(&level->entities, i);
		if (e != NULL && e->update != NULL) {
			e->draw(e, level, context);
		}
	}

	player_draw(&level->player, level, context);

	rlPopMatrix();
}

#pragma endregion

#pragma region Player Functions

void player_init(player_t* player) {
	*player = (player_t) {
        .sprites_index = mario_sprites.idle,
        .is_big = true
    };
	physics_body_init(&player->body, 8, 18);
}

inline sprite_t* player_get_sprite(player_t* player) {
	return (player->sprites_index == NULL) ? NULL : &player->sprites_index[player->is_big];
}

void player_animate(player_t* player, controller_state_t* controller) {
	// crouch
	if (player->is_crouching) {
		player->sprites_index = mario_sprites.crouch;
		player->image_index = 0;
		return;
	}

	// jump
	if (!player->body.grounded) {
		player->sprites_index = mario_sprites.jump;
		player->image_index = player->body.yspd > 0 ? 1 : 0;
		return;
	}

	// skid
	if (controller->current.h * player->body.xspd < 0.0f) {
		player->sprites_index = mario_sprites.skid;
		return;
	}

	// walk
	if (player->body.xspd != 0.0f) {
		player->sprites_index = mario_sprites.walk;
		player->image_index += MAX(0.125f, fabsf(player->body.xspd) / 6.0f);
		return;
	}

	// idle
	player->sprites_index = mario_sprites.idle;
	player->image_index = (controller->current.v < 0) ? 1 : 0;
}

void player_draw(player_t* player, level_t* level, render_context_t* context) {
	if (player->sprites_index != NULL) {
		sprite_t* sprite_index = &player->sprites_index[player->is_big];
		sprite_draw_ex(sprite_index, player->image_index, player->body.x, player->body.y + 1, sprite_index->width * player->body.origin_x, sprite_index->height * player->body.origin_y, player->flip_x, false, context);
	}

	Rectangle bounds = physics_body_get_rectangle(&player->body);
	bounds = (Rectangle) { floorf(bounds.x), floorf(bounds.y), bounds.width, bounds.height };
	DrawRectangleLinesEx(bounds, 1, RED);
	DrawPixel(floorf(player->body.x), floorf(player->body.y), BLUE);
}

void player_jump(player_t* player) {
	float variable_jump = (fabsf(player->body.xspd / PLAYER_RUN_SPEED)) * 1.0f; // normalize jump between 0 and 1 based on player speed from walk to full height
	player->body.yspd = -PLAYER_JUMP - variable_jump;
	PlaySound(sounds.jump);
}

void player_move(player_t* player, level_t* level, controller_state_t* controller) {
	// A lower gravity is used to make the player jump higher if the player is variable jumping
	player->body.grav = (controller->current.a) ? PLAYER_GRAVITY_HOLD : PLAYER_GRAVITY;

	if (player->body.grounded && controller->current.a && !controller->previous.a) {
		player_jump(player);
	}

	if (player->body.grounded) {
		if (controller->current.v > 0) {
			if (!player->is_crouching) {
				player->is_crouching = true;
				player->body.height = 10;
			}
			controller->current.h = 0;
		}
		else {
			if (player->is_crouching) {
				player->is_crouching = false;
				player->body.height = 18;
			}
		}
	}

	float h = controller->current.h;
	float traction = 1.0f; // ice would be something like 0.2 for snowy levels

	// player speed can be changed any time on ground, or when the player is in the air and sign of input = spd
	if ((player->body.grounded) || ((!player->body.grounded && h * player->body.xspd >= 0))) {
		if (controller->current.b && fabsf(player->body.xspd) >= PLAYER_WALK_SPEED && h * player->body.xspd > 0) {
			player->body.xspd_max = PLAYER_RUN_SPEED;
		}
		else {
			player->body.xspd_max = PLAYER_WALK_SPEED;
		}
	}
	else {
		player->body.xspd_max = 0.0f;
	}

	// horizontal input when less than max speed
	if (h != 0) {
		if (h * player->body.xspd < player->body.xspd_max) {
			player->flip_x = (h < 0) ? true : false;
			// normal forward acceleration
			if (h * player->body.xspd > 0) {
				player->body.xspd += h * PLAYER_ACCEL;
			}
			// skidding
			else {
				// ground skid
				if (player->body.grounded) {
					// multiplies the turn speed by an amount
					float skid_factor = 1.0f;
					float current_xspd = fabsf(player->body.xspd);
					if (current_xspd <= PLAYER_WALK_SPEED) {
						skid_factor = 1.0f;
					}
					else if (current_xspd <= PLAYER_RUN_SPEED) {
						skid_factor = 2.0f;
					}
					else if (current_xspd > PLAYER_RUN_SPEED) {
						skid_factor = 4.0f;
					}
					player->body.xspd += h * (PLAYER_TURN * skid_factor * traction);
				}
				else if (h * player->body.xspd > -PLAYER_WALK_SPEED) {
					player->body.xspd += h * PLAYER_TURN;
				}
				else {
					player->body.xspd += h * (PLAYER_TURN_AIR * 2);
				}
			}
		}
	}
	// no input held down- slow player
	else if (player->body.xspd != 0) {
		float xspd_dir = (player->body.xspd > 0) ? 1.0f : -1.0f; // which way the player is moving
		player->body.xspd -= ((player->body.grounded) ? PLAYER_DECEL : PLAYER_DECEL_AIR) * xspd_dir;
		if (player->body.xspd * xspd_dir < 0) {
			player->body.xspd = 0;
		}
	}

	physics_body_update(&player->body, &level->tilemap);
}

void player_update(player_t* player, level_t* level, controller_state_t* controller) {
	player_move(player, level, controller);
	player_animate(player, controller);
}

#pragma endregion

int main(int argc, char** argv) {
	game_t game;
	game_init(WINDOW_CAPTION, &game);
	game_end(&game);
}