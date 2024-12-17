#include <stdlib.h>
#include <stdio.h>
#include <raylib.h>
#include <string.h>
#include "../external/stb/stb_rect_pack.h"

#define DEBUG

typedef unsigned int id_t;

// game related defines
#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 224
#define MAX_CONTROLLERS 4

// texture atlas defines
#define TEXTURE_ATLAS_WIDTH 1024
#define TEXTURE_ATLAS_HEIGHT 1024
#define MAX_TEXTURE_NODES 1024

// array list defines
#define ARRAYLIST_NULL -1
#define ARRAYLIST_SCALE_FACTOR 2

// data defines
#ifdef DEBUG
#define printd(...) printf("[DEBUG] " __VA_ARGS__);
#else
#define printd(...)
#endif

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
		printd("Freeing frames\n");
		free(sprite->frames);
	}
	if (sprite->order) {
		printd("Freeing order\n");
		free(sprite->order);
	}
}

void sprite_draw(sprite_t* sprite, int image_index, float x, float y, Texture2D atlas) {
	int order_index = (sprite->order == NULL) ? image_index % sprite->frame_count : sprite->order[image_index % sprite->order_count];
	printf("imageindex: %d, orderindex: %d\n", image_index, order_index);

	Rectangle src = sprite->frames[order_index].source;
	Rectangle dest = (Rectangle) { x, y, src.width, src.height };
	printf("w: %f\n", src.width);
	DrawTexturePro(atlas, src, dest, (Vector2) { 0.0f, 0.0f }, 0, WHITE);
}

// Modify the sprite_animation_init function
void sprite_init(const char* fname, sprite_t* anim, Image* atlas_img, stbrp_context* context) {
	char indexed_fname[512];
	int fname_len = strlen(fname);
	memcpy(indexed_fname, fname, fname_len + 1);
	for (int i = 0; i < fname_len; ++i) {
		if (fname[i] == '.') {
			indexed_fname[i] = '/';
		}
	}
	indexed_fname[fname_len] = '\0';

    anim->frame_count = 0;
    anim->order_count = 0;
    anim->order = NULL;
    anim->frames = NULL;
    
    char image_path[512], data_path[512];

    snprintf(image_path, sizeof(image_path), "assets/sprites/%s.png", indexed_fname);
    snprintf(data_path, sizeof(data_path), "assets/sprites/%s.dat", indexed_fname);

    Image img = LoadImage(image_path);
    int sprite_width = img.width, sprite_height = img.height;

    // Load the .dat file and initialize the animation frames and order (same as before)
    FILE* file = fopen(data_path, "r");
	if (file == NULL) {
		anim->frame_count = sprite_height / sprite_width;
		anim->frames = malloc(anim->frame_count * sizeof(sprite_frame_t));
		for (int i = 0; i < anim->frame_count; ++i) {
			stbrp_rect r = { 0, sprite_width, sprite_width };
			if (stbrp_pack_rects(context, &r, 1)) {
				ImageDraw(atlas_img, img, (Rectangle) { 0, (i * sprite_width), sprite_width, sprite_width }, (Rectangle) { r.x, r.y, sprite_width, sprite_width }, WHITE);
				anim->frames[i].source = (Rectangle) { (float)r.x, (float)r.y, (float)r.w, (float)r.h };
				printd("%s.%d: [%f, %f, %f, %f]\n", fname, i, anim->frames[i].source.x, anim->frames[i].source.y, anim->frames[i].source.width, anim->frames[i].source.height);
			}
		}
		UnloadImage(img);
		return;
	}
    
    char order_buf[128] = "";
    char line[256];

    while (fgets(line, sizeof(line), file)) {
        if (anim->frame_count == 0 && strncmp(line, "frames:", 7) == 0) {
            if (sscanf(line, "frames: %d", &anim->frame_count) != 1) {
                goto close_file;
            } else {
                anim->frames = malloc(anim->frame_count * sizeof(sprite_frame_t));
                printd("Frames: %d\n", anim->frame_count);
                sprite_height = img.height / anim->frame_count;
                printd("Frame dimensions: [%d, %d]\n", sprite_width, sprite_height);

				for (int i = 0; i < anim->frame_count; ++i) {
					stbrp_rect r = { 0, sprite_width, sprite_height };
					if (stbrp_pack_rects(context, &r, 1)) {
						ImageDraw(atlas_img, img, (Rectangle) { 0, (int)(i * sprite_height), sprite_width, sprite_height }, (Rectangle) { r.x, r.y, sprite_width, sprite_height }, WHITE);
						anim->frames[i].source = (Rectangle) { (float)r.x, (float)r.y, (float)r.w, (float)r.h };
						printd("%s.%d: [%f, %f, %f, %f]\n", fname, i, anim->frames[i].source.x, anim->frames[i].source.y, anim->frames[i].source.width, anim->frames[i].source.height);
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
                printd("Order (%d): [", anim->order_count);

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
                        printd("%d", temp);
                        if (num_count < anim->order_count) {
                            printd(", ");
                        }
                    } else i++;
                }
                printd("]\n");
            }
        }
    }
close_file:
    fclose(file);
}

#pragma endregion

#pragma region Text

typedef enum font { font_hud, font_textbox } font_t;

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
	float xspd;
	float yspd;
	float grav;
} physics_t;

bool collision_rectangle(Rectangle rec1, Rectangle rec2) {
	return CheckCollisionRecs(rec1, rec2);
}

bool collision_rectangles(Rectangle rec1, Rectangle* recs, int num_rects) {
	for (int i = 0; i < num_rects; ++i) {
		if (collision_rectangle(rec1, recs[i])) {
			return true;
		}
	}
	return false;
}

#pragma endregion

#pragma region Level

typedef struct level {
	int width;
	int height;
	arraylist_t enemies;
} level_t;

void level_init(level_t* level) {
	arraylist_init(&level->enemies, 64);
}

void level_free(level_t* level) {
	arraylist_free(&level->enemies);
}

#pragma endregion

#pragma region Enemies

typedef enum enemy_type {
	ENEMY_GOOMBA = 0,
	ENEMY_KOOPA,
	ENEMY_PIRANHA,
	ENEMY_COUNT
} enemy_type_t;

#pragma region Base entity type

typedef struct enemy {
	id_t id;
	enemy_type_t type;
	physics_t phys;
	bool is_active;
	void (*update)(struct enemy*, struct level*);
	void (*draw)(struct enemy*, struct level*);
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

void goomba_update(enemy_t* enemy, level_t* level) {
	enemy_goomba_t* g = (enemy_goomba_t*)enemy;
}

void goomba_draw(enemy_t* enemy, level_t* level) {

}

#pragma endregion

#pragma endregion

#pragma region Game

sprite_t spr_idle_big, spr_walk_big, spr_jump_big;

typedef struct game {
	int controller_count;
	Texture2D texture_atlas;
	controller_state_t* controllers;
	controller_state_t* controllers_previous;
	level_t* level;
} game_t;

void game_update(game_t* game) {
	// update controllers
	for (int i = 0; i < 1 /* stand in for controller count since we're only accounting for p1 atm */; ++i) {
		memcpy(&game->controllers_previous[i], &game->controllers[i], sizeof(controller_state_t));
		controller_state_t* state = &game->controllers[i];
		controller_state_update(state);
	}

	for (int i = 0; i < game->level->enemies.count; ++i) {
		enemy_t* e = (enemy_t*)game->level->enemies.data[i];
	}
}

float t;
void game_draw(game_t* game) {
	sprite_draw(&spr_walk_big, t, -8, 0, game->texture_atlas);
	t += 0.2;
}

void game_start(game_t* game) {
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

	// controller set-up
	game->controller_count = 1;
	game->controllers = malloc(MAX_CONTROLLERS * sizeof(controller_state_t));
	game->controllers_previous = malloc(MAX_CONTROLLERS * sizeof(controller_state_t));
	if (game->controllers && game->controllers_previous) {
		for (int i = 0; i < MAX_CONTROLLERS; ++i) {
			game->controllers[i].id = i;
		}
	}

	// first level init
	game->level = malloc(sizeof(level_t));
	level_init(game->level);

	// start window
	InitWindow(SCREEN_WIDTH * 4, SCREEN_HEIGHT * 4, "");
	SetTargetFPS(60);

	Image atlas_img = GenImageColor(TEXTURE_ATLAS_WIDTH, TEXTURE_ATLAS_HEIGHT, (Color) { 0, 0, 0, 0 });

	{
		stbrp_context c;
		stbrp_node n[MAX_TEXTURE_NODES];
		stbrp_init_target(&c, TEXTURE_ATLAS_WIDTH, TEXTURE_ATLAS_HEIGHT, n, MAX_TEXTURE_NODES);

		sprite_init("mario.idle_big", &spr_idle_big, &atlas_img, &c);
		sprite_init("mario.walk_big", &spr_walk_big, &atlas_img, &c);
		sprite_init("mario.jump_big", &spr_jump_big, &atlas_img, &c);
	}

	game->texture_atlas = LoadTextureFromImage(atlas_img);

	UnloadImage(atlas_img);

	goomba.base.update((enemy_t*)&goomba, game->level);

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
		DrawTexture(game->texture_atlas, 0, 0, WHITE);
		EndDrawing();
	}
	UnloadRenderTexture(render_texture);
}

void game_end(game_t* game) {
	UnloadTexture(game->texture_atlas);

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