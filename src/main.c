#include <stdlib.h>
#include <stdio.h>
#include <raylib.h>
#include <stb_rect_pack.h>
#include <string.h>
#include <math.h>

#pragma region Sounds [ Temporary. Should be passed by context ]

struct sounds {
	Sound bump;
	Sound jump;
} sounds;

#pragma endregion

#pragma region Defines

#define DEBUG

typedef long entity_id_t;

// resource related defines
#define SPRITES_PATH 		"assets/sprites"
#define SOUNDS_PATH 		"assets/sounds"
#define BACKGROUNDS_PATH 	"assets/backgrounds"
#define MAX_PATH_LEN 256

// game related defines
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 224
#define MAX_CONTROLLERS 4

#define ORANGE_SKY	(Color) { 255, 231, 181, 255 }
#define BLUE_SKY	(Color) { 0, 99, 189, 255 }

// texture atlas defines
#define TEXTURE_ATLAS_WIDTH 512
#define TEXTURE_ATLAS_HEIGHT 512
#define MAX_TEXTURE_NODES 1024

// array list defines
#define ARRAYLIST_NULL -1
#define ARRAYLIST_SCALE_FACTOR 2

#ifdef DEBUG
#define printd(...) printf(__VA_ARGS__);
#else
#define printd(...)
#endif

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

inline float clampf(float val, float min, float max) {
	if (val < min) {
		return min;
	}
	return (max > val) ? val : max;
}

int clamp(int val, int min, int max) {
	if (val < min) {
		return min;
	}
	return (max > val) ? val : max;
}
double clampd(double val, double min, double max) {
	if (val < min) {
		return min;
	}
	return (max > val) ? val : max;
}

int rand_int(int min, int max) {
	return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

#pragma endregion

#pragma region IO

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

#pragma region Array List

#define ARRAYLIST_DEFINE(type, type_name) \
typedef struct type_name { type* data; int count; int capacity; } type_name##_arraylist_t; \
void type_name##_arraylist_init(type_name##_arraylist_t* list, int initial_capacity) { \
    list->count = 0; \
    list->capacity = initial_capacity; \
    list->data = malloc(list->capacity * sizeof(type)); \
} \
void type_name##_arraylist_free(type_name##_arraylist_t* list) { \
    list->count = list->capacity = 0; \
    free(list->data); \
} \
void type_name##_arraylist_push(type_name##_arraylist_t* list, type data) { \
    if (list->count >= list->capacity) { \
        list->capacity *= ARRAYLIST_SCALE_FACTOR; \
        list->data = realloc(list->data, list->capacity * sizeof(type)); \
    } \
    list->data[list->count] = data; \
    list->count++; \
} \
type type_name##_arraylist_get(type_name##_arraylist_t* list, int index) { \
    return (index < 0 || index >= list->count) ? (type){ 0 } : list->data[index]; \
} \
void type_name##_arraylist_remove(type_name##_arraylist_t* list, int index) { \
    if (index < 0 || index >= list->count) return; \
    for (int i = index; i < list->count - 1; i++) list->data[i] = list->data[i + 1]; /* pushes data to the left */ \
    --list->count; \
}

ARRAYLIST_DEFINE(Rectangle, rectangle)

#pragma endregion

#pragma region Tilemap

typedef enum collision_type { COLLISION_AIR, COLLISION_SEMI, COLLISION_SOLID } collision_type_t;

#define TILEMAP_DEFINE(type, type_name, default_val) \
typedef struct type_name##_tilemap { \
	type** data; \
	int width; \
	int height; \
	int tile_size; \
} type_name##_tilemap_t; \
void type_name##_tilemap_init(type_name##_tilemap_t* map, int width, int height) { \
	map->width = width; \
	map->height = height; \
	map->tile_size = 16; \
	map->data = malloc(width * sizeof(type*)); \
	for (int x = 0; x < width; ++x) { \
		map->data[x] = calloc(height, sizeof(type)); \
	} \
} \
void type_name##_tilemap_free(type_name##_tilemap_t* map) { \
	for (int x = 0; x < map->width; ++x) { \
		free(map->data[x]); \
	} \
	free(map->data); \
} \
inline type type_name##_tilemap_get(type_name##_tilemap_t* map, int x, int y) { \
	if (x < 0 || y < 0 || x >= map->width || y >= map->height) { \
		return default_val; \
	} \
	return map->data[x][y]; \
} \
inline void type_name##_tilemap_set(type_name##_tilemap_t* map, int x, int y, type val) { \
	map->data[x][y] = val; \
}

TILEMAP_DEFINE(collision_type_t, collision, COLLISION_AIR)

#pragma endregion

#pragma region Control States

typedef struct controller_buttons {
	int id;
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
	state->current.a = IsKeyDown(KEY_X);
	state->current.b = IsKeyDown(KEY_Z);
	state->current.x = IsKeyDown(KEY_C);
	state->current.y = IsKeyDown(KEY_V);
	state->current.h = IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT);
	state->current.v = IsKeyDown(KEY_DOWN) - IsKeyDown(KEY_UP);
}

#pragma endregion

#pragma region Render Context

struct render_context {
	Texture2D sprite_atlas;
};

#pragma endregion

#pragma region Backgrounds

typedef struct background {
	float x, y;
	float parallax_x;
	float parallax_y;
	Texture2D tex;
	bool clamp_x;
	bool clamp_y;
} background_t;

void background_init(const char* res_loc, background_t* background, bool tiled) {
	background->x = background->y = 0;
	background->parallax_x = background->parallax_y = 0;
	background->clamp_x = false;
	background->clamp_y = false;

	// index path
	char indexed_loc[MAX_PATH_LEN] = "";
	path_index(res_loc, indexed_loc);

	// get full bg path
	char img_path[MAX_PATH_LEN] = "";
	snprintf(img_path, sizeof(img_path), BACKGROUNDS_PATH "/%s.png", indexed_loc);

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

void background_draw(background_t* background) {
	int bg_x = (int)(background->x * background->parallax_x) % background->tex.width;

	Rectangle bg_rect = (Rectangle) { 0, 0, GetScreenWidth(), GetScreenHeight() };
	Rectangle screen_rect = (Rectangle){ bg_x, background->y * background->parallax_y, bg_rect.width, bg_rect.height };
	if (background->clamp_y) {
		if (screen_rect.y < -background->tex.height + SCREEN_HEIGHT) {
			screen_rect.y = -background->tex.height + SCREEN_HEIGHT;
		} else if (screen_rect.y > 0) {
			screen_rect.y = 0;
		}
	}
	DrawTexturePro(background->tex, bg_rect, screen_rect, (Vector2) { 0, 0 }, 0, WHITE);
}

void background_free(background_t* background) {
	UnloadTexture(background->tex);
}

#pragma endregion

#pragma region Sprites

typedef struct sprite_frame {
	int x, y;
} sprite_frame_t;

typedef struct sprite {
	int width, height;
	int frame_count;
	sprite_frame_t* frames;
	int order_count;
	int* order;
} sprite_t;

void sprite_free(sprite_t* sprite) {
	if (sprite->frames) {
		free(sprite->frames);
	}
	if (sprite->order) {
		free(sprite->order);
	}
}

const sprite_frame_t sprite_get_frame(sprite_t* sprite, int image_index) {
	return (sprite->order == NULL) ?
		sprite->frames[image_index % sprite->frame_count] :					// no custom order; get frame from image index
		sprite->frames[sprite->order[image_index % sprite->order_count]];	// custom order; get frame from order index
}

void sprite_draw_pro(sprite_t* sprite, int image_index, float x, float y, int origin_x, int origin_y, bool flip_x, bool flip_y, render_context_t* context) {
	// frame to utilize from sprite frames
	sprite_frame_t frame = sprite_get_frame(sprite, image_index);
	Rectangle sprite_rect = (Rectangle){ floorf(frame.x), floorf(frame.y), sprite->width, sprite->height };
	if (flip_x) {
		sprite_rect.width = -sprite_rect.width;
	}
	Rectangle screen_rect = (Rectangle){ floorf(x), floorf(y), sprite_rect.width, sprite_rect.height };
	DrawTexturePro(context->sprite_atlas, sprite_rect, screen_rect, (Vector2) { origin_x, origin_y }, 0, WHITE);
}

inline void sprite_draw(sprite_t* sprite, int image_index, float x, float y, bool flip_x, bool flip_y, render_context_t* context) {
	sprite_draw_pro(sprite, image_index, x, y, 0.0f, 0.0f, flip_x, flip_y, context);
}

void sprite_init(const char* res_loc, sprite_t* sprite, Image* atlas_img, stbrp_context* rect_packer) {
	// replace all instances of "." with "/" for local resources
	char indexed_fname[MAX_PATH_LEN] = "";
	path_index(res_loc, indexed_fname);

	sprite->frame_count = sprite->order_count = 0;
    sprite->order = NULL;
	sprite->frames = NULL;
    
    char image_path[MAX_PATH_LEN] = "", data_path[MAX_PATH_LEN] = "";
	snprintf(image_path, sizeof(image_path), SPRITES_PATH "/%s.png", indexed_fname);
	snprintf(data_path, sizeof(data_path), SPRITES_PATH "/%s.dat", indexed_fname);

    Image img = LoadImage(image_path);
    int sprite_width = img.width, sprite_height = img.height;

    // load .dat file and initialize the animation frames and order
    FILE* file = fopen(data_path, "r");
	if (file == NULL) {
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

    while (fgets(line, sizeof(line), file)) {
        if (sprite->frame_count == 0 && strncmp(line, "frames:", 7) == 0) {
            if (sscanf(line, "frames: %d", &sprite->frame_count) != 1) {
                goto close_file;
            } else {
                sprite->frames = malloc(sprite->frame_count * sizeof(sprite_frame_t));
                sprite_height = img.height / sprite->frame_count;
                printd("Frame dimensions: [%d, %d]\n", sprite_width, sprite_height);

				sprite->width = sprite_width;
				sprite->height = sprite_height;
				for (int i = 0; i < sprite->frame_count; ++i) {
					stbrp_rect r = { 0, sprite_width, sprite_height };
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
            } else {
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
                    } else i++;
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
                    } else i++;
                }
            }
        }
    }

close_file:
#ifdef DEBUG
	if (sprite->order) {
		printd("Loaded sprite [%s] with [%d] frames and pre-defined order of [", res_loc, sprite->frame_count);
		for (int i = 0; i < sprite->order_count; ++i) {
			printd("%d", sprite->order[i]);
			if (i < sprite->order_count - 1) {
				printd(", ");
			}
		}
		printd("]\n");
	} else {
		printd("Loaded sprite [%s] with [%d] frames\n", res_loc, sprite->frame_count);
	}
#endif
    fclose(file);
}

#pragma endregion

#pragma region Text

typedef struct font {
	sprite_t sprite_data;
	const char order[256];
	int spacing;
} font_t;

font_t fnt_hud;

void font_init(const char* res_loc, const char* order, font_t* font, Image* atlas, stbrp_context* rect_packer) {
	sprite_init(res_loc, &font->sprite_data, atlas, rect_packer);
	font->spacing = 0;
	int order_len = strlen(order);
	memcpy((void*)font->order, order, (size_t)order_len + 1);
#ifdef DEBUG
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

void font_free(font_t* font) {
	sprite_free(&font->sprite_data);
}

void text_draw(const char* text, font_t* font, float x, float y, render_context_t* context) {
	size_t string_len = strlen(text), order_len = strlen(font->order);
	int _x = x, _y = y;
	int font_width = font->sprite_data.width;
	for (int i = 0; i < string_len; ++i) {
		if (text[i] == ' ') {
			_x += 8;
			continue;
		}
		if (text[i] == '\n') {
			_x = 0;
			_y += 8;
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

typedef enum powerup { POWERUP_SMALL, POWERUP_BIG, POWERUP_FIRE, } powerup_t;

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

Texture2D cheat_tileset;

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

void physics_body_init(physics_body_t* body, int width, int height) {
	body->xspd = 0.0f;
	body->yspd = 0.0f;

	body->xspd_max = 10.0f;
	body->yspd_max = 5.0f;

	body->origin_x = 0.5f;
	body->origin_y = 1.0f;

	body->x = 0.0f;
	body->y = 0.0f;

	body->grav = 0.2f;

	body->grounded = false;

	body->width = width;
	body->height = height;
}

Rectangle physics_body_get_rectangle(physics_body_t* body) {
	return (Rectangle) { body->x - body->width * body->origin_x, body->y - (body->height * body->origin_y), body->width, body->height };
}

void physics_body_update(physics_body_t* body) {
	body->yspd += body->grav;
	// y fall speed clamp
	if (body->yspd > body->yspd_max) {
		body->yspd = body->yspd_max;
	}
	// right speed clamp
	if (body->xspd > body->xspd_max) {
		body->xspd = body->xspd_max;
	}
	// left speed clamp
	if (body->xspd < -body->xspd_max) {
		body->xspd = -body->xspd_max;
	}
	// apply motion
	body->x += body->xspd;
	body->y += body->yspd;
}

bool point_in_rectangle(Vector2 p, Rectangle r) {
	return CheckCollisionPointRec(p, r);
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

void resolve_collisions_x(physics_body_t* body, collision_tilemap_t* map) {
	Rectangle body_rect = physics_body_get_rectangle(body);
	int tile_size = map->tile_size;

	int left = body_rect.x / tile_size, right = (body_rect.x + body_rect.width) / tile_size;
	int top = body_rect.y / tile_size, bottom = (body_rect.y + body_rect.height) / tile_size;

	for (int y = top - 1; y <= bottom + 1; ++y) {
		// right
		if (body->xspd > 0) {
			Rectangle side_player_rect = { body_rect.x + body_rect.width - body->xspd, body_rect.y, body->xspd * 2, body_rect.height };

			for (int x = right; x <= right + 1; ++x) {
				collision_type_t tile_type = collision_tilemap_get(map, x, y);
				if (tile_type == COLLISION_AIR) continue;

				Rectangle tile_rect = { x * tile_size, y * tile_size, tile_size, tile_size };
				if (rectangle_collision(side_player_rect, tile_rect)) {
					body->x = tile_rect.x - body_rect.width + (body->width / 2);
					body->xspd = 0;
				}
			}
		}
		// left
		else if (body->xspd < 0) {
			Rectangle side_player_rect = { body_rect.x + body->xspd, body_rect.y, -body->xspd * 2, body_rect.height };

			for (int x = left - 1; x <= left; ++x) {
				collision_type_t tile_type = collision_tilemap_get(map, x, y);
				if (tile_type == COLLISION_AIR) continue;

				Rectangle tile_rect = { x * tile_size, y * tile_size, tile_size, tile_size };
				if (rectangle_collision(side_player_rect, tile_rect)) {
					body->x = tile_rect.x + tile_size + (body->width / 2);
					body->xspd = 0;
				}
			}
		}
	}
}

inline void resolve_collisions_y(physics_body_t* body, collision_tilemap_t* map) {
	Rectangle body_rect = physics_body_get_rectangle(body);
	int tile_size = map->tile_size;

	int left = body_rect.x / tile_size, right = (body_rect.x + body_rect.width) / tile_size;
	int top = body_rect.y / tile_size, bottom = (body_rect.y + body_rect.height) / tile_size;

	body->grounded = false;
	for (int x = left - 1; x <= right + 1; ++x) {
		// up
		if (body->yspd < 0) {
			for (int y = top - 1; y <= top; ++y) {
				collision_type_t tile_type = collision_tilemap_get(map, x, y);
				if (tile_type == COLLISION_AIR) continue;

				Rectangle tile_rect = { x * tile_size, y * tile_size, tile_size, tile_size };
				if (rectangle_collision(body_rect, tile_rect)) {
					body->y = tile_rect.y + tile_size + body->height;
					body->yspd = 0;
					PlaySound(sounds.bump);
					return;
				}
			}
		}
		// down
		else for (int y = bottom; y <= bottom + 1; ++y) {
			collision_type_t tile_type = collision_tilemap_get(map, x, y);
			if (tile_type == COLLISION_AIR) continue;

			Rectangle tile_rect = { x * tile_size, y * tile_size, tile_size, tile_size };
			if (rectangle_collision(body_rect, tile_rect)) {
				if (body->yspd > 0) {
					body->y = tile_rect.y;
					body->yspd = 0;
					body->grounded = true;
					return;
				}
			}
		}
	}
}

#pragma endregion

#pragma region Entity

typedef enum entity_type {
	ENTITY_NONE = -1,
	ENTITY_GOOMBA,
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
	powerup_t powerup;
	sprite_t* sprites_index;
	bool flip_x;
	float image_index;
};

#pragma endregion

#pragma region Level Struct, Init, & Free

struct level {
	player_t player;
	entityptr_arraylist_t entities;
	Color background_color;
	background_t background;
	collision_tilemap_t collision_map;
	entity_id_t next_entity_id;
	int camera_x;
	int camera_y;
};

void level_init(level_t* level, const char* background_res, Color background_color, int width, int height) {
	// entities
	player_init(&level->player);
	entityptr_arraylist_init(&level->entities, 64);
	level->next_entity_id = 0;
	
	// bg
	background_init(background_res, &level->background, true);
	level->background.y = -level->background.tex.height + SCREEN_HEIGHT;
	level->background.clamp_x = false;
	level->background.clamp_y = true;
	level->background.parallax_x = 0.5f;
	level->background.parallax_y = 1.0f;
	level->background_color = background_color;

	// tilemap (temporary, oh my god. delegate to a file champ)
	collision_tilemap_init(&level->collision_map, width, height);
	for (int i = 0; i <= 7; ++i) {
		collision_tilemap_set(&level->collision_map, i, 14, COLLISION_SOLID);
	}
	for (int i = 8; i <= 12; ++i) {
		collision_tilemap_set(&level->collision_map, i, 15, COLLISION_SOLID);
	}
	for (int i = 13; i <= 14; ++i) {
		collision_tilemap_set(&level->collision_map, i, 14, COLLISION_SOLID);
	}
	collision_tilemap_set(&level->collision_map, 15, 13, COLLISION_SOLID);
	collision_tilemap_set(&level->collision_map, 15, 12, COLLISION_SOLID);
	for (int i = 17; i <= 19; ++i) {
		collision_tilemap_set(&level->collision_map, i, 11, COLLISION_SOLID);
	}
	collision_tilemap_set(&level->collision_map, 16, 12, COLLISION_SOLID);
	for (int i = 20; i <= 23; ++i) {
		collision_tilemap_set(&level->collision_map, i, 10, COLLISION_SOLID);
	}
	collision_tilemap_set(&level->collision_map, 0, 10, COLLISION_SOLID);
	for (int i = 2; i <= 5; ++i) {
		collision_tilemap_set(&level->collision_map, i, 9, COLLISION_SOLID);
	}
	for (int i = 6; i <= 10; ++i) {
		collision_tilemap_set(&level->collision_map, i, 8, COLLISION_SOLID);
	}
	for (int i = 5; i <= 7; ++i) {
		collision_tilemap_set(&level->collision_map, 10, i, COLLISION_SOLID);
	}
}

void level_free(level_t* level) {
	if (level->entities.data != NULL) {
		for (int i = 0; i < level->entities.count; ++i) {
			if (level->entities.data[i] != NULL) {
				free(level->entities.data[i]);
			}
		}
	}
	entityptr_arraylist_free(&level->entities);
	background_free(&level->background);
	collision_tilemap_free(&level->collision_map);
}

#pragma endregion

#pragma region Entity Functions

void entity_init(entity_t* entity, entity_type_t type, level_t* level, int width, int height) {
	physics_body_init(&entity->body, width, height);
	entity->id = level->next_entity_id++;
	entity->type = type;
	entity->is_active = false;
	entity->update = NULL;
	entity->draw = NULL;
}

void entity_update(entity_t* entity, level_t* level) {
	// foobuh barfew lorem ipsum sum checksum ugugghh
}

int compare_entities(const void* a, const void* b) {
	const entity_t* entity_a = (const entity_t*)a;
	const entity_t* entity_b = (const entity_t*)b;
	// returns 0 if the ID matches
	return entity_a->id - entity_b->id;
}

#pragma endregion

#pragma region Enemies

typedef struct entity_goomba {
	entity_t base;
} entity_goomba_t;

void goomba_init(entity_goomba_t* entity, level_t* level) {
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
	// update controllers
	for (int i = 0; i < 1 /* stand in for controller count since we're only accounting for p1 */; ++i) {
		controller_state_t* state = &game->controllers[i];
		controller_state_update(state);
	}

	if (game->level) {
		level_update(game->level, game);
	}
}

void game_draw(game_t* game) {
	if (game->level) {
		level_draw(game->level, &game->render_context);
	}
}

void game_init(game_t* game) {
	// start window
	SetTraceLogLevel(LOG_NONE);
	InitWindow(SCREEN_WIDTH * 4, SCREEN_HEIGHT * 4, "");
	InitAudioDevice();
	SetTargetFPS(60);
	sounds.bump = LoadSound("assets/sounds/bump.wav");
	sounds.jump = LoadSound("assets/sounds/jump.wav");

	// initialization would be something like this for an entity
	entity_goomba_t goomba = { {
		.id = 1,
		.type = ENTITY_GOOMBA,
		.body = { 0 },
		.is_active = false,
		.update = goomba_update,
		.draw = goomba_draw
	} };

	// controller set-up
	game->controller_count = 1;
	game->controllers = malloc(MAX_CONTROLLERS * sizeof(controller_state_t));

	// atlas image
	// todo: asset loading automation!
	Image atlas_img = GenImageColor(TEXTURE_ATLAS_WIDTH, TEXTURE_ATLAS_HEIGHT, (Color) { 255, 255, 255, 0 });
	{
		stbrp_context rect_packer;
		stbrp_node* n = malloc(sizeof(stbrp_node) * MAX_TEXTURE_NODES);
		stbrp_init_target(&rect_packer, TEXTURE_ATLAS_WIDTH, TEXTURE_ATLAS_HEIGHT, n, MAX_TEXTURE_NODES);

		sprite_init("mario.idle_small", &mario_sprites.idle[POWERUP_SMALL], &atlas_img, &rect_packer);
		sprite_init("mario.walk_small", &mario_sprites.walk[POWERUP_SMALL], &atlas_img, &rect_packer);
		sprite_init("mario.run_small", &mario_sprites.run[POWERUP_SMALL], &atlas_img, &rect_packer);
		sprite_init("mario.jump_small", &mario_sprites.jump[POWERUP_SMALL], &atlas_img, &rect_packer);
		sprite_init("mario.skid_small", &mario_sprites.skid[POWERUP_SMALL], &atlas_img, &rect_packer);

		sprite_init("mario.idle_big", &mario_sprites.idle[POWERUP_BIG], &atlas_img, &rect_packer);
		sprite_init("mario.walk_big", &mario_sprites.walk[POWERUP_BIG], &atlas_img, &rect_packer);
		sprite_init("mario.run_big", &mario_sprites.run[POWERUP_BIG], &atlas_img, &rect_packer);
		sprite_init("mario.jump_big", &mario_sprites.jump[POWERUP_BIG], &atlas_img, &rect_packer);
		sprite_init("mario.skid_big", &mario_sprites.skid[POWERUP_BIG], &atlas_img, &rect_packer);

		font_init("font.hud", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.,*-!@|=:", &fnt_hud, &atlas_img, &rect_packer);
		fnt_hud.spacing = 0;

		cheat_tileset = LoadTexture("assets/tiles/cheat.png");

		free(n);
	}

	game->render_context.sprite_atlas = LoadTextureFromImage(atlas_img);

	ExportImage(atlas_img, "sprite_atlas_dump.png");
	UnloadImage(atlas_img);

	// first level init
	game->level = malloc(sizeof(level_t));
	level_init(game->level, "cave", (Color) { 0, 0, 0, 255 }, 24, 16);

	// create rendering surface 
	RenderTexture2D render_texture = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
	while (!WindowShouldClose()) {
		// update
		game_update(game);

		// draw game to render target
		BeginTextureMode(render_texture);
		if (game->level != NULL) {
			ClearBackground(game->level->background_color);
		}
		game_draw(game);
		EndTextureMode();
		
		// draw render target to screen
		BeginDrawing();
		ClearBackground(WHITE);
		DrawTexturePro(render_texture.texture, (Rectangle) { 0, 0, render_texture.texture.width, -render_texture.texture.height }, (Rectangle) { 0, 0, GetScreenWidth(), -GetScreenHeight() }, (Vector2) { 0, 0 }, 0, WHITE);
		EndDrawing();
	}
	UnloadRenderTexture(render_texture);
}

void game_end(game_t* game) {
	UnloadTexture(game->render_context.sprite_atlas);

	free(game->controllers);

	level_free(game->level);
	free(game->level);

	CloseWindow();
}

#pragma endregion

#pragma region Level Update & Draw

void level_update(level_t* level, game_t* game) {
	// update player before entities
	player_update(&level->player, level, &game->controllers[0]);
	
	// cycle thru entities & run their updates
	for (int i = 0; i < level->entities.count; ++i) {
		entity_t* e = entityptr_arraylist_get(&level->entities, i);
		if (e != NULL) {
			entity_update(e, level);	// all entity update
			if (e->update != NULL) {	// uniquely assigned update
				e->update(e, level);
			}
		}
	}

	level->camera_x = level->player.body.x;
	level->camera_y = level->player.body.y;

	// camera update
	int cam_x = (int)level->camera_x;
	int cam_y = (int)level->camera_y;

	int lw = level->collision_map.width * level->collision_map.tile_size;
	int lh = level->collision_map.height * level->collision_map.tile_size;

	// clamp to level bounds
	int half_width = SCREEN_WIDTH / 2;
	int half_height = SCREEN_HEIGHT / 2;
	// x clamp
	if (cam_x < half_width) {
		cam_x = half_width;
	}
	else if (cam_x > lw - half_width) {
		cam_x = lw - half_width;
	}
	// y clamp
	if (cam_y < half_height) {
		cam_y = half_height;
	}
	else if (cam_y > lh - half_height) {
		cam_y = lh - half_height;
	}

	level->camera_x = cam_x - half_width;
	level->camera_y = cam_y - half_height;
}

void level_draw(level_t* level, render_context_t* context) {
	level->background.x = -level->camera_x;
	background_draw(&level->background);
	DrawTexture(cheat_tileset, -level->camera_x, -level->camera_y, (Color){ 255, 255, 255, 255 });

	for (int i = 0; i < level->entities.count; ++i) {
		entity_t* e = level->entities.data[i];
		if (e != NULL && e->update != NULL) {
			e->update(e, level);
		}
	}
	player_draw(&level->player, level, context);

	char dtr[512];
	snprintf(dtr, 512, "PX:%d,PY:%d", (int)(level->player.body.x), (int)(level->player.body.y));
	text_draw(dtr, &fnt_hud, 0, 0, context);
	snprintf(dtr, 512, "E:%d", level->entities.count);
	text_draw(dtr, &fnt_hud, 0, 8, context);
}

#pragma endregion

#pragma region Player Functions

void player_init(player_t* player) {
	physics_body_init(&player->body, 8, 18);
	player->powerup = POWERUP_BIG;
	player->sprites_index = mario_sprites.idle;
	player->image_index = 0;
	player->flip_x = false;
}

inline sprite_t* player_get_sprite(player_t* player) {
	return (player->sprites_index == NULL) ? NULL : &player->sprites_index[player->powerup];
}

void player_draw(player_t* player, level_t* level, render_context_t* context) {
	// render player
	if (player->sprites_index != NULL) {
		sprite_t* sprite_index = &player->sprites_index[player->powerup];
		sprite_draw_pro(sprite_index, player->image_index, player->body.x - level->camera_x, player->body.y + 1 - level->camera_y, sprite_index->width * player->body.origin_x, sprite_index->height * player->body.origin_y, player->flip_x, false, context);
	}
}

void player_update(player_t* player, level_t* level, controller_state_t* controller) {
	static const float decel = 0.0625f;
	static const float decel_air = 0.0125f;
	static const float accel = 0.09375f;
	static const float turn = 0.15625f;
	static const float turn_air = 0.15625f;
	static const float grav_jump = 0.1875f;
	static const float grav_fall = 0.375f;

	player->image_index += 0.125f;
	if (player->body.yspd > 0 || player->body.yspd < 0 && !controller->current.a) {
		player->body.grav = grav_fall;
	}
	else {
		player->body.grav = grav_jump;
	}

	if (player->body.grounded && controller->current.a && !controller->previous.a) {
		player->body.yspd = -5.0f - (fabsf(player->body.xspd / 2.25f));
		PlaySound(sounds.jump);
	}
	else if (player->body.yspd > 5.0f) {
		player->body.yspd = 5.0f;
	}

	float h = controller->current.h;
	int traction = 1;
	// horizontal max speed changes under these conditions:
	// (not crouching, and on the ground) OR
	// (not on the ground, and ((xspd >= 0 & h > 0) | (xspd <= 0 & h < 0))
	if ((player->body.grounded) ||
		((!player->body.grounded) && ((player->body.xspd >= 0 && h > 0) || (player->body.xspd <= 0 && h < 0)))) {
		if (fabsf(player->body.xspd) >= 1.25f && controller->current.b) {
			player->body.xspd_max = 2.25f;
		}
		else {
			player->body.xspd_max = 1.25f;
		}
	}
	else {
		player->body.xspd_max = 0.0f;
	}

	// player is moving left or right
	if (h != 0 && (h * player->body.xspd < player->body.xspd_max)) {
		player->flip_x = (h < 0) ? true : false;
		// forward acceleration just increments by the acceleration value
		if (h * player->body.xspd >= 0) {
			player->body.xspd += h * accel;
		}
		// skidding
		else {
			// ground skid
			if (player->body.grounded) {
				// multiplies the turn speed by an amount
				float skid_factor = 1.0f;
				if (fabsf(player->body.xspd) <= 1.25f) {
					skid_factor = 1.0f;
				}
				else if (fabsf(player->body.xspd) <= 2.25f) {
					skid_factor = 2.0f;
				}
				else if (fabsf(player->body.xspd) > 2.25f) {
					skid_factor = 4.0f;
				}
				player->body.xspd += h * ((turn * skid_factor) * traction);

			}
			else if (h * player->body.xspd > -1.25f) {
				player->body.xspd += h * turn;
			}
			else
				player->body.xspd += h * (turn_air * 2);
		}
	}
	else {
		if (player->body.xspd > 0) {
			player->body.xspd -= (player->body.grounded) ? decel : decel_air;
			if (player->body.xspd < 0) {
				player->body.xspd = 0;
			}
		}
		else if (player->body.xspd < 0) {
			player->body.xspd += (player->body.grounded) ? decel : decel_air;
			if (player->body.xspd > 0) {
				player->body.xspd = 0;
			}
		}
	}

	// a sign function could concat these functions rly easily
	if (player->body.grounded) {
		if (player->body.xspd > player->body.xspd_max) {
			player->body.xspd = player->body.xspd_max;
		}
		else if (player->body.xspd < -player->body.xspd_max) {
			player->body.xspd = -player->body.xspd_max;
		}
	}

	player->body.yspd += player->body.grav;
	player->body.x += player->body.xspd;
	resolve_collisions_x(&player->body, &level->collision_map);
	player->body.y += player->body.yspd;
	resolve_collisions_y(&player->body, &level->collision_map);

	// put this in an animation update section?
	if (!player->body.grounded) {
		player->sprites_index = mario_sprites.jump;
		player->image_index = player->body.yspd > 0 ? 1 : 0;
	}
	else {
		if (player->body.xspd != 0) {
			player->sprites_index = mario_sprites.walk;
			player->image_index += fabsf(player->body.xspd) / 8.0f;
		}
		else {
			player->sprites_index = mario_sprites.idle;
			player->image_index = (controller->current.v < 0) ? 1 : 0;
		}
		if (h != 0 && h * player->body.xspd < 0) {
			player->sprites_index = mario_sprites.skid;
		}
	}
}

#pragma endregion

int main(int argc, char** argv) {
	game_t game;
	game_init(&game);
	game_end(&game);
}