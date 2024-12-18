#include <stdlib.h>
#include <stdio.h>
#include <raylib.h>
#include <stb_rect_pack.h>
#include <string.h>
#include <math.h>

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

#pragma region Forward Declares

struct controller_state;
typedef struct controller_state controller_state_t;

struct game;
typedef struct game game_t;

struct render_context;
typedef struct render_context render_context_t;

struct entity;
typedef struct entity entity_t;

struct level;
typedef struct level level_t;
void level_update(level_t*, game_t*);
void level_draw(level_t*, render_context_t*);

struct player;
typedef struct player player_t;
void player_init(player_t*);
void player_update(player_t* player, level_t* level, controller_state_t* controller);
void player_draw(player_t*, render_context_t*);

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

#define ARRAYLIST_DEFINE(type, signifier) \
typedef struct signifier { type* data; int count; int capacity; } signifier##_t; \
void signifier##_init(signifier##_t* list, int initial_capacity) { \
	printd("type: " #type "\n"); \
    list->count = 0; \
    list->capacity = initial_capacity; \
    list->data = malloc(list->capacity * sizeof(type)); \
} \
void signifier##_free(signifier##_t* list) { \
    list->count = list->capacity = 0; \
    free(list->data); \
} \
void signifier##_push(signifier##_t* list, type data) { \
    if (list->count >= list->capacity) { \
        list->capacity *= ARRAYLIST_SCALE_FACTOR; \
        list->data = realloc(list->data, list->capacity * sizeof(type)); \
    } \
    list->data[list->count] = data; \
    list->count++; \
} \
type signifier##_get(signifier##_t* list, int index) { \
    return (index < 0 || index >= list->count) ? (type){ 0 } : list->data[index]; \
} \
void signifier##_remove(signifier##_t* list, int index) { \
    if (index < 0 || index >= list->count) return; \
    for (int i = index; i < list->count - 1; i++) list->data[i] = list->data[i + 1]; /* pushes data to the left */ \
    --list->count; \
}

ARRAYLIST_DEFINE(Rectangle, rectangle_arraylist)

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
	Rectangle source;
} sprite_frame_t;

typedef struct sprite {
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

void sprite_draw(sprite_t* sprite, int image_index, float x, float y, render_context_t* context) {
	// frame to utilize from sprite frames
	int frame_index = (sprite->order == NULL) ?
		image_index % sprite->frame_count :					// no custom order; get frame from image index
		sprite->order[image_index % sprite->order_count];	// custom order; get frame from order index

	Rectangle sprite_rect = sprite->frames[frame_index].source;
	Rectangle screen_rect = (Rectangle) { x, y, sprite_rect.width, sprite_rect.height };
	DrawTexturePro(context->sprite_atlas, sprite_rect, screen_rect, (Vector2) { 0.0f, 0.0f }, 0, WHITE);
}

void sprite_init(const char* res_loc, sprite_t* anim, Image* atlas_img, stbrp_context* rect_packer) {
	// replace all instances of "." with "/" for local resources
	char indexed_fname[MAX_PATH_LEN] = "";
	path_index(res_loc, indexed_fname);

    anim->frame_count = anim->order_count = 0;
    anim->order = NULL;
	anim->frames = NULL;
    
    char image_path[MAX_PATH_LEN] = "", data_path[MAX_PATH_LEN] = "";
	snprintf(image_path, sizeof(image_path), SPRITES_PATH "/%s.png", indexed_fname);
	snprintf(data_path, sizeof(data_path), SPRITES_PATH "/%s.dat", indexed_fname);

    Image img = LoadImage(image_path);
    int sprite_width = img.width, sprite_height = img.height;

    // load .dat file and initialize the animation frames and order
    FILE* file = fopen(data_path, "r");
	if (file == NULL) {
		anim->frame_count = ceilf(sprite_height / (float)sprite_width);
		anim->frames = malloc(anim->frame_count * sizeof(sprite_frame_t));
		int frame_height = sprite_width > sprite_height ? sprite_height : sprite_width;
		for (int i = 0; i < anim->frame_count; ++i) {
			stbrp_rect r = { 0, sprite_width, frame_height };
			if (stbrp_pack_rects(rect_packer, &r, 1)) {
				ImageDraw(atlas_img, img, (Rectangle) { 0, (i * frame_height), sprite_width, frame_height }, (Rectangle) { r.x, r.y, sprite_width, frame_height }, WHITE);
				anim->frames[i].source = (Rectangle) { (float)r.x, (float)r.y, (float)r.w, (float)r.h };
			}
		}
		UnloadImage(img);
		printd("Loaded sprite [%s] with [%d] frames\n", res_loc, anim->frame_count);
		return;
	}
    
    char order_buf[256] = "";
    char line[256] = "";

    while (fgets(line, sizeof(line), file)) {
        if (anim->frame_count == 0 && strncmp(line, "frames:", 7) == 0) {
            if (sscanf(line, "frames: %d", &anim->frame_count) != 1) {
                goto close_file;
            } else {
                anim->frames = malloc(anim->frame_count * sizeof(sprite_frame_t));
                sprite_height = img.height / anim->frame_count;
                printd("Frame dimensions: [%d, %d]\n", sprite_width, sprite_height);

				for (int i = 0; i < anim->frame_count; ++i) {
					stbrp_rect r = { 0, sprite_width, sprite_height };
					if (stbrp_pack_rects(rect_packer, &r, 1)) {
						ImageDraw(atlas_img, img, (Rectangle) { 0, (int)(i * sprite_height), sprite_width, sprite_height }, (Rectangle) { r.x, r.y, sprite_width, sprite_height }, WHITE);
						anim->frames[i].source = (Rectangle) { (float)r.x, (float)r.y, (float)r.w, (float)r.h };
					}
				}
            }
        }
        if (anim->order == NULL && strncmp(line, "order:", 6) == 0) {
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
                        anim->order_count++;
                    } else i++;
                }

                anim->order = malloc(anim->order_count * sizeof(int));

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
                        anim->order[num_count++] = temp;
                    } else i++;
                }
            }
        }
    }

close_file:
#ifdef DEBUG
	if (anim->order) {
		printd("Loaded sprite [%s] with [%d] frames and pre-defined order of [", res_loc, anim->frame_count);
		for (int i = 0; i < anim->order_count; ++i) {
			printd("%d", anim->order[i]);
			if (i < anim->order_count - 1) {
				printd(", ");
			}
		}
		printd("]\n");
	} else {
		printd("Loaded sprite [%s] with [%d] frames\n", res_loc, anim->frame_count);
	}
#endif
    fclose(file);
}

#pragma endregion

#pragma region Text

typedef enum font_type { font_hud, font_textbox } font_type_t;

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

void text_draw(const char* text, font_t* font, float x, float y, render_context_t* context, int limit) {
	size_t string_len = strlen(text), order_len = strlen(font->order);
	int _x = x, _y = y;
	for (int i = 0; i < string_len && i < limit; ++i) {
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
				sprite_draw(&font->sprite_data, j, _x, _y, context);
				_x += font->sprite_data.frames[j].source.width + font->spacing;;
				break;
			}
		}
	}
}

#pragma endregion

#pragma region Animations

typedef enum powerups { POWERUP_SMALL, POWERUP_BIG, POWERUP_FIRE, } powerups_t;

struct {
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

#pragma region Physics Components

typedef struct physics_body {
	int x, y;
	int width, height;
	float xspd, yspd;
	float xspd_max, yspd_max;
	float grav;
} physics_body_t;

void physics_body_init(physics_body_t* body, int width, int height) {
	body->xspd = 0.0f;
	body->yspd = 0.0f;
	body->xspd_max = 10.0f;
	body->yspd_max = 5.0f;
	body->grav = 0.2f;
	body->width = width;
	body->height = height;
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

bool collision_rectangle(Rectangle rec1, Rectangle rec2) {
	return CheckCollisionRecs(rec1, rec2);
}

bool collision_rectangles(Rectangle base_rect, Rectangle* recs, int num_rects) {
	for (int i = 0; i < num_rects; ++i) {
		if (collision_rectangle(base_rect, recs[i])) {
			return true;
		}
	}
	return false;
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
	id_t id;
	entity_type_t type;
	physics_body_t body;
	bool is_active;
	void (*update)(struct entity*, struct level*);
	void (*draw)(struct entity*, struct level*, render_context_t* context);
};

ARRAYLIST_DEFINE(entity_t*, entity_arraylist)

#pragma endregion

#pragma region Player

struct player {
	physics_body_t body;
	float img_index;
};

void player_init(player_t* player) {
	physics_body_init(&player->body, 8, 14);
	player->img_index = 0;
}

Rectangle player_rectangle(player_t* player) {
	return (Rectangle) { player->body.x + 8, player->body.y, 16, 32 };
}

#pragma endregion

#pragma region Tilemap

typedef enum collision_type { COLLISION_AIR, COLLISION_SEMI, COLLISION_SOLID } collision_type_t;

typedef struct tilemap {
	collision_type_t** data;
	int width;
	int height;
	int tile_size;
} tilemap_t;

void tilemap_init(tilemap_t* map, int width, int height) {
	map->width = width;
	map->height = height;
	map->tile_size = 16;
	map->data = malloc(width * sizeof(collision_type_t*));
	for (int x = 0; x < width; ++x) {
		map->data[x] = calloc(height, sizeof(collision_type_t));
	}
}

void tilemap_free(tilemap_t* map) {
	for (int x = 0; x < map->width; ++x) {
		free(map->data[x]);
	}
	free(map->data);
}

inline collision_type_t tilemap_get(tilemap_t* map, int x, int y) {
	if (x < 0 || y < 0 || x >= map->width || y >= map->height) {
		return COLLISION_AIR;
	}
	return map->data[x][y];
}

#define tilemap_set(map, x, y, val) map.data[x][y] = val

#pragma endregion

#pragma region Level Init & Free

struct level {
	player_t player;
	entity_arraylist_t entities;
	Color background_color;
	background_t background;
	tilemap_t collision_map;
	entity_id_t next_entity_id;
};

void level_init(level_t* level, const char* background_res, Color background_color, int width, int height) {
	player_init(&level->player);
	entity_arraylist_init(&level->entities, 64);
	level->background.clamp_x = false;
	level->background.clamp_y = true;
	level->next_entity_id = 0;

	background_init(background_res, &level->background, true);
	level->background_color = background_color;
	tilemap_init(&level->collision_map, width, height);

	for (int x = 0; x < width; ++x) {
		for (int y = 0; y < height; ++y) {
			if (x < 8 && x > 3 && y >= height - 3) {
				tilemap_set(level->collision_map, x, y, COLLISION_SEMI);
			} else if (y >= height - 2) {
				tilemap_set(level->collision_map, x, y, COLLISION_SEMI);
			} else {
				tilemap_set(level->collision_map, x, y, COLLISION_AIR);
			}
		}
		printf("\n");
	}
}

void level_free(level_t* level) {
	for (int i = 0; i < level->entities.count; ++i) {
		if (level->entities.data[i] != NULL) {
			free(level->entities.data[i]);
		}
	}
	entity_arraylist_free(&level->entities);
	background_free(&level->background);
	tilemap_free(&level->collision_map);
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

void game_start(game_t* game) {
	// start window
	SetTraceLogLevel(LOG_NONE);
	InitWindow(SCREEN_WIDTH * 4, SCREEN_HEIGHT * 4, "");
	SetTargetFPS(60);

	// initialization would be something like this
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

	// first level init
	game->level = malloc(sizeof(level_t));
	level_init(game->level, "plains", BLUE_SKY, 20, 14);

	Image atlas_img = GenImageColor(TEXTURE_ATLAS_WIDTH, TEXTURE_ATLAS_HEIGHT, (Color) { 255, 255, 255, 0 });
	{
		stbrp_context rect_packer;
		stbrp_node* n = malloc(sizeof(stbrp_node) * MAX_TEXTURE_NODES);
		stbrp_init_target(&rect_packer, TEXTURE_ATLAS_WIDTH, TEXTURE_ATLAS_HEIGHT, n, MAX_TEXTURE_NODES);

		sprite_init("mario.idle_small", &mario_sprites.idle[POWERUP_SMALL], &atlas_img, &rect_packer);
		sprite_init("mario.walk_small", &mario_sprites.walk[POWERUP_SMALL], &atlas_img, &rect_packer);
		sprite_init("mario.run_small", &mario_sprites.run[POWERUP_SMALL], &atlas_img, &rect_packer);
		sprite_init("mario.jump_small", &mario_sprites.jump[POWERUP_SMALL], &atlas_img, &rect_packer);

		sprite_init("mario.idle_big", &mario_sprites.idle[POWERUP_BIG], &atlas_img, &rect_packer);
		sprite_init("mario.walk_big", &mario_sprites.walk[POWERUP_BIG], &atlas_img, &rect_packer);
		sprite_init("mario.run_big", &mario_sprites.run[POWERUP_BIG], &atlas_img, &rect_packer);
		sprite_init("mario.jump_big", &mario_sprites.jump[POWERUP_BIG], &atlas_img, &rect_packer);

		font_init("font.hud", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.,*-!@|=:", &fnt_hud, &atlas_img, &rect_packer);
		fnt_hud.spacing = 0;

		free(n);
	}

	game->render_context.sprite_atlas = LoadTextureFromImage(atlas_img);

	ExportImage(atlas_img, "sprite_atlas_dump.png");
	UnloadImage(atlas_img);

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
	player_update(&level->player, level, &game->controllers[0]);
	/*{
		entity_t* e = malloc(sizeof(entity_t));
		entity_arraylist_push(&level->entities, e);
	}*/
	for (int i = 0; i < level->entities.count; ++i) {
		entity_t* e = entity_arraylist_get(&level->entities, i);
		if (e != NULL) {
			entity_update(e, level);	// all entity update
			if (e->update != NULL) {	// uniquely assigned update
				e->update(e, level);
			}
		}
	}
}

void level_draw(level_t* level, render_context_t* context) {
	level->background.parallax_x = 0.5f;
	level->background.parallax_y = 1.0f;
	level->background.y = -level->background.tex.height + SCREEN_HEIGHT;
	level->background.x -= 1.25f;
	background_draw(&level->background);
	for (int x = 0; x < level->collision_map.width; ++x) {
		for (int y = 0; y < level->collision_map.height; ++y) {
			if (level->collision_map.data[x][y] != COLLISION_AIR) {
				DrawRectangle(x * 16, y * 16, 16, 16, (Color) { 255, 128, 0, 190 });
			}
		}
	}
	for (int i = 0; i < level->entities.count; ++i) {
		entity_t* e = level->entities.data[i];
		if (e != NULL && e->update != NULL) {
			e->update(e, level);
		}
	}
	player_draw(&level->player, context);
}

#pragma endregion

#pragma region Player Update

void player_draw(player_t* player, render_context_t* context) {
	static int text_val = 0;
	sprite_t* render_sprite = mario_sprites.jump;
	player->img_index = (player->body.yspd > 0) ? 1 : 0;
	if (player->body.y >= SCREEN_HEIGHT - 32) {
		render_sprite = mario_sprites.idle;
	}

	Rectangle prect = player_rectangle(player);
	int tile_size = 16;
	rectangle_arraylist_t collision_list;
	rectangle_arraylist_init(&collision_list, 1);
	int left = prect.x / tile_size, right = (prect.x + prect.width) / tile_size;
	int top = prect.y / tile_size, bottom = (prect.y + prect.height) / tile_size;
	for (int x = left - 1; x <= right + 1; ++x) {
		for (int y = top - 1; y <= bottom + 1; ++y) {
			rectangle_arraylist_push(&collision_list, (Rectangle) { x * 16, y * 16, 16, 16 });
		}
	}
	for (int i = 0; i < collision_list.capacity; ++i) {
		Rectangle pos_rect = rectangle_arraylist_get(&collision_list, i);
		DrawRectanglePro(pos_rect, (Vector2) { 0, 0 }, 0, (Color) { 255, 0, 0, 255 });
	}
	printd("%d\n", collision_list.count);
	rectangle_arraylist_free(&collision_list);
	DrawRectanglePro(prect, (Vector2) { 0, 0 }, 0, (Color) { 0, 255, 255, 255 });

	if (render_sprite != NULL) {
		sprite_draw(&render_sprite[POWERUP_BIG], player->img_index, player->body.x, player->body.y, context);
	}
	text_draw("WELCOME TO DINOSAUR LAND!    \nIN THIS FAR AWAY LAND, \nMARIO HAS FOUND HIMSELF \nFACE-TO-FACE WITH NEW FRIENDS,     \nAND TERRIFYING ENEMIES!", &fnt_hud, 0, 0, context, (text_val++) / 3.0f);
}

void player_update(player_t* player, level_t* level, controller_state_t* controller) {
	player->img_index += 0.125f;
	player->body.grav = 0.3750;
	if (controller->current.a) {
		if (!controller->previous.a) {
			player->body.yspd = -5.0f;
		} else {
			player->body.grav = 0.1875;
		}
	}
	physics_body_update(&player->body);

	if (player->body.y > SCREEN_HEIGHT - 32) {
		player->body.y = SCREEN_HEIGHT - 32;
	}
}

#pragma endregion

int main(int argc, char** argv) {
	game_t game;
	game_start(&game);
	game_end(&game);
}