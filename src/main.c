#include <stdlib.h>
#include <stdio.h>
#include <raylib.h>
#include <stb_rect_pack.h>
#include <string.h>

#define DEBUG

typedef unsigned int id_t;

// resource related defines
#define SPRITES_PATH "assets/sprites"
#define SOUNDS_PATH "assets/sounds"
#define BACKGROUNDS_PATH "assets/backgrounds"
#define MAX_PATH_LEN 256

// game related defines
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 224
#define MAX_CONTROLLERS 4

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

struct game;
typedef struct game game_t;

struct level;
typedef struct level level_t;

struct player;
typedef struct player player_t;
void player_init(player_t*);
void player_update(player_t*, level_t*);
void player_draw(player_t*, game_t*);

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
	return dest_loc;
}

#pragma endregion

#pragma region Array List

typedef struct arraylist {
	void** data; // array of pointers to store the elements
	int count;
	int capacity;
} arraylist_t;

void arraylist_init(arraylist_t* list, int initial_capacity) {
	list->count = 0;
	list->capacity = initial_capacity;
	list->data = malloc(list->capacity * sizeof(void*));
}

void arraylist_free(arraylist_t* list) {
	list->count = 0;
	list->capacity = 0;
	free(list->data);
}

void arraylist_push(arraylist_t* list, void* data) {
	if (list->count >= list->capacity) {
		list->capacity *= ARRAYLIST_SCALE_FACTOR;
		list->data = realloc(list->data, list->capacity * sizeof(void*));
	}
	list->data[list->count] = data;
	list->count++;
}

void arraylist_pop(arraylist_t* list) {
	if (list->count > 0) {
		list->count--;
	}
}

void arraylist_remove(arraylist_t* list, int index) {
	if (index < 0 || index >= list->count) {
		return;
	}

	for (int i = index; i < list->count - 1; i++) {
		list->data[i] = list->data[i + 1];
	}

	list->count--;
}

int arraylist_find(arraylist_t* list, void* element, int (*cmp)(const void*, const void*)) {
	for (int i = 0; i < list->count; i++) {
		if (cmp(list->data[i], element) == 0) {
			return i;	// element found, return calculated index
		}
	}
	return ARRAYLIST_NULL;
}

void* arraylist_get(arraylist_t* list, int index) {
	if (index < 0 || index >= list->count) {
		return NULL;
	}
	return list->data[index];
}

#pragma endregion

#pragma region Render Context

typedef struct render_context {
	Texture2D sprite_atlas;
} render_context_t;

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
	if (indexed_loc) {
		snprintf(img_path, sizeof(img_path), BACKGROUNDS_PATH "/%s.png", indexed_loc);
	}

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
	int bg_x = (int)floor(background->x * background->parallax_x) % background->tex.width;

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

sprite_t
spr_idle_small,
spr_walk_small,
spr_run_small,
spr_jump_small,

spr_idle_big,
spr_walk_big,
spr_run_big,
spr_jump_big;

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

// Modify the sprite_animation_init function
void sprite_init(const char* res_loc, sprite_t* anim, Image* atlas_img, stbrp_context* rect_packer) {
	// replace all instances of "." with "/" for local resources
	char indexed_fname[MAX_PATH_LEN] = "";
	path_index(res_loc, indexed_fname);

    anim->frame_count = anim->order_count = 0;
    anim->order = anim->frames = NULL;
    
    char image_path[MAX_PATH_LEN] = "", data_path[MAX_PATH_LEN] = "";
	if (indexed_fname) {
		snprintf(image_path, sizeof(image_path), SPRITES_PATH "/%s.png", indexed_fname);
		snprintf(data_path, sizeof(data_path), SPRITES_PATH "/%s.dat", indexed_fname);
	}

    Image img = LoadImage(image_path);
    int sprite_width = img.width, sprite_height = img.height;

    // Load the .dat file and initialize the animation frames and order (same as before)
    FILE* file = fopen(data_path, "r");
	if (file == NULL) {
		anim->frame_count = ceil(sprite_height / (float)sprite_width);
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

                // 
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
    fclose(file);
}

#pragma endregion

#pragma region Text

typedef enum font_type { font_hud, font_textbox } font_type_t;

typedef struct font {
	sprite_t sprite_data;
	const char order[256];
} font_t;

font_t fnt_hud;

void font_init(const char* res_loc, const char* order, font_t* font, Image* atlas, stbrp_context* rect_packer) {
	sprite_init(res_loc, &font->sprite_data, atlas, rect_packer);
	int order_len = strlen(order);
	memcpy(font->order, order, order_len + 1);
	printd("Font [%s] order: [", res_loc);
	for (int i = 0; i < order_len; ++i) {
		printd("%c", font->order[i]);
		if (i < order_len - 1) {
			printd(", ");
		}
	}
	printd("]\n");
}

void font_free(font_t* font) {
	sprite_free(&font->sprite_data);
}

void text_draw(const char* text, font_t* font, float x, float y, render_context_t* context, int limit) {
	size_t string_len = strlen(text);
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
		int index = 0;
		for (int j = 0; j < strlen(font->order); ++j) {
			const char pos_char = font->order[j];
			if (pos_char == text[i]) {
				sprite_draw(&font->sprite_data, j, _x, _y, context);
			}
		}
		_x += 8;
	}
}

#pragma endregion

#pragma region Control States

typedef struct controller_state {
	int id;
	int h, v;
	bool a, b, x, y;
	bool l, r;
} controller_state_t;

void controller_state_update(controller_state_t* state) {
	state->a = IsKeyDown(KEY_X);
	state->b = IsKeyDown(KEY_Z);
	state->x = IsKeyDown(KEY_C);
	state->y = IsKeyDown(KEY_V);
	state->h = IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT);
	state->v = IsKeyDown(KEY_DOWN) - IsKeyDown(KEY_UP);
}

#pragma endregion

#pragma region Physics Components

typedef struct physics {
	float xspd, yspd;
	float grav;
} physics_t;

void physics_init(physics_t* phys) {

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

#pragma region Player

typedef struct player {
	physics_t phys;
	float img_index;
} player_t;

void player_init(player_t* player) {
	physics_init(&player->phys);
	player->img_index = 0;
}

void player_update(player_t* player, level_t* level) {
	player->img_index += 0.125f;
}

void player_draw(player_t* player, render_context_t* context) {
	sprite_draw(&spr_walk_big, player->img_index, 0 + 16, 0 + 170, context);
	sprite_draw(&spr_walk_small, player->img_index * 1.25f, 8 + 16, 8 + 170, context);
	text_draw("WELCOME TO DINOSAUR LAND!    \nIN THIS FAR AWAY LAND, \nMARIO HAS FOUND HIMSELF \nFACE-TO-FACE WITH NEW FRIENDS,     \nAND TERRIFYING ENEMIES!", &fnt_hud, 0, 0, context, player->img_index * 2.0);
}

#pragma endregion

#pragma region Level

typedef struct level {
	player_t player;
	arraylist_t enemies;
	background_t background;
	int width, height;
} level_t;

void level_init(level_t* level) {
	player_init(&level->player);
	arraylist_init(&level->enemies, 64);
	background_init("plains", &level->background, true);
	level->background.clamp_x = false;
	level->background.clamp_y = true;
}

void level_free(level_t* level) {
	arraylist_free(&level->enemies);
	background_free(&level->background);
}

#pragma endregion

#pragma region Enemies

typedef enum enemy_type {
	ENEMY_GOOMBA = 0,
	ENEMY_KOOPA,
	ENEMY_PIRANHA,
	ENEMY_COUNT
} enemy_type_t;

#pragma region Base enemy type

typedef struct enemy {
	id_t id;
	enemy_type_t type;
	physics_t phys;
	bool is_active;
	void (*update)(struct enemy*, struct level*);
	void (*draw)(struct enemy*, struct level*, render_context_t* context);
} enemy_t;

int compare_enemies(const void* a, const void* b) {
	const enemy_t* enemy_a = (const enemy_t*)a;
	const enemy_t* enemy_b = (const enemy_t*)b;
	// returns 0 if the ID matches
	return enemy_a->id - enemy_b->id;
}

#pragma endregion

#pragma region Goomba

typedef struct enemy_goomba {
	enemy_t base;
} enemy_goomba_t;

void goomba_init(enemy_goomba_t* enemy) {
	physics_init(&enemy->base.phys);
}

void goomba_update(enemy_t* enemy, level_t* level) {
	enemy_goomba_t* g = (enemy_goomba_t*)enemy;
}

void goomba_draw(enemy_t* enemy, level_t* level, render_context_t* context) {
	enemy_goomba_t* g = (enemy_goomba_t*)enemy;
}

#pragma endregion

#pragma endregion

#pragma region Game

typedef struct game {
	int controller_count;
	render_context_t render_context;
	controller_state_t* controllers;
	controller_state_t* controllers_previous;
	level_t* level;
} game_t;

void game_update(game_t* game) {
	// update controllers
	for (int i = 0; i < 1 /* stand in for controller count since we're only accounting for p1 */; ++i) {
		memcpy(&game->controllers_previous[i], &game->controllers[i], sizeof(controller_state_t));
		controller_state_t* state = &game->controllers[i];
		controller_state_update(state);
	}

	if (game->level) {
		level_t* level = game->level;

		player_update(&level->player, level);

		for (int i = 0; i < game->level->enemies.count; ++i) {
			enemy_t* e = (enemy_t*)game->level->enemies.data[i];
			e->update(e, game->level);
		}
	}
}

void game_draw(game_t* game) {
	if (game->level) {
		level_t* level = game->level;
		
		level->background.parallax_x = 0.5f;
		level->background.parallax_y = 1.0f;
		level->background.y = -level->background.tex.height + SCREEN_HEIGHT;
		level->background.x -= 1.25f;
		background_draw(&level->background);
		for (int i = 0; i < game->level->enemies.count; ++i) {
			enemy_t* e = (enemy_t*)game->level->enemies.data[i];
			e->update(e, game->level);
		}
		player_draw(&level->player, &game->render_context);
	}
}

void game_start(game_t* game) {
	// start window
	SetTraceLogLevel(LOG_NONE);
	InitWindow(SCREEN_WIDTH * 4, SCREEN_HEIGHT * 4, "");
	SetTargetFPS(60);

	/*
	// initialization would be something like this
	enemy_goomba_t goomba = {
		.base = {
			.id = 1,
			.type = ENEMY_GOOMBA,
			.phys = { 0 },
			.is_active = false,
			.update = goomba_update,
			.draw = goomba_draw
		}
	};
	*/

	// controller set-up
	game->controller_count = 1;
	game->controllers = malloc(MAX_CONTROLLERS * sizeof(controller_state_t));
	game->controllers_previous = malloc(MAX_CONTROLLERS * sizeof(controller_state_t));
	if (game->controllers != NULL && game->controllers_previous != NULL) {
		for (int i = 0; i < MAX_CONTROLLERS; ++i) {
			// set ID ref
			game->controllers[i].id = i;
		}
	}

	// first level init
	game->level = malloc(sizeof(level_t));
	level_init(game->level);

	Image atlas_img = GenImageColor(TEXTURE_ATLAS_WIDTH, TEXTURE_ATLAS_HEIGHT, (Color) { 255, 255, 255, 0 });
	{
		stbrp_context rect_packer;
		stbrp_node* n = malloc(sizeof(stbrp_node) * MAX_TEXTURE_NODES);
		stbrp_init_target(&rect_packer, TEXTURE_ATLAS_WIDTH, TEXTURE_ATLAS_HEIGHT, n, MAX_TEXTURE_NODES);

		sprite_init("mario.idle_small", &spr_idle_small, &atlas_img, &rect_packer);
		sprite_init("mario.walk_small", &spr_walk_small, &atlas_img, &rect_packer);
		sprite_init("mario.run_small", &spr_run_small, &atlas_img, &rect_packer);
		sprite_init("mario.jump_small", &spr_jump_small, &atlas_img, &rect_packer);

		sprite_init("mario.idle_big", &spr_idle_big, &atlas_img, &rect_packer);
		sprite_init("mario.walk_big", &spr_walk_big, &atlas_img, &rect_packer);
		sprite_init("mario.run_big", &spr_run_big, &atlas_img, &rect_packer);
		sprite_init("mario.jump_big", &spr_jump_big, &atlas_img, &rect_packer);

		font_init("font.hud", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.,*-!@|=:", &fnt_hud, &atlas_img, &rect_packer);

		free(n);
	}

	game->render_context.sprite_atlas = LoadTextureFromImage(atlas_img);

	ExportImage(atlas_img, "sprite_atlas_dump.png");
	UnloadImage(atlas_img);

	//goomba.base.update((enemy_t*)&goomba, game->level);

	// create rendering surface 
	RenderTexture2D render_texture = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
	while (!WindowShouldClose()) {
		// update
		game_update(game);

		// draw game to render target
		BeginTextureMode(render_texture);
		ClearBackground(BLACK);
		game_draw(game);
		EndTextureMode();
		
		// draw render target to screen
		BeginDrawing();
		ClearBackground(WHITE);
		DrawTexturePro(render_texture.texture, (Rectangle) { 0, 0, render_texture.texture.width, -render_texture.texture.height }, (Rectangle) { 0, 0, GetScreenWidth(), -GetScreenHeight() }, (Vector2) { 0, 0 }, 0, WHITE);
		//DrawTexture(game->render_context.sprite_atlas, 0, 0, WHITE);
		EndDrawing();
	}
	UnloadRenderTexture(render_texture);
}

void game_end(game_t* game) {
	UnloadTexture(game->render_context.sprite_atlas);

	free(game->controllers);
	free(game->controllers_previous);

	level_free(game->level);
	free(game->level);

	CloseWindow();
}

#pragma endregion

int main(int argc, char** argv) {
	game_t game;
	game_start(&game);
	game_end(&game);
}